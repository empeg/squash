/*
 * Copyright 2003 by Adam Luter
 * This file is part of Squash, a C/Ncurses-based unix music player.
 *
 * Squash is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Squash is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Squash; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * player.c
 */

#include "global.h"
#include "sound.h"      /* for sound_*() */
#include "play_flac.h"  /* for flac_*() */
#include "play_mp3.h"   /* for mp3_*() */
#include "play_ogg.h"   /* for ogg_*() */
#include "spectrum.h"   /* for spectrum_reset() */
#include "stat.h"       /* for feedback() */
#include "player.h"

/*
 * Define the song functions
 */
song_functions_t song_functions[] = {
    { NULL, NULL, NULL, NULL, NULL },
    { ogg_open, ogg_decode_frame, ogg_calc_duration, ogg_seek, ogg_close },
    { mp3_open, mp3_decode_frame, mp3_calc_duration, mp3_seek, mp3_close },
    { flac_open, flac_decode_frame, flac_calc_duration, flac_seek, flac_close }
};

void *frame_decoder( void *input_data ) {
    frame_data_t new_frame;
    void *decoder_data = NULL;
    frame_data_t(* decoder_function)(void *) = NULL;
    void (*close_function)(void *) = NULL;
    bool have_new_frame = FALSE;

    while( 1 ) {
        /* decode some data */
        if( decoder_function ) {
            new_frame = decoder_function( decoder_data );
            have_new_frame = TRUE;
        }

        squash_lock( frame_buffer.lock );

        if( frame_buffer.song_eof ) {
            if( close_function ) {
                close_function( decoder_data );
                close_function = NULL;
                decoder_function = NULL;
                frame_buffer.song_eof = FALSE;
            }
        }

        if( have_new_frame ) {
            have_new_frame = FALSE;
            if( new_frame.pcm_size == 0 || new_frame.pcm_size <= -2 ) {
                if( close_function ) {
                    close_function( decoder_data );
                    close_function = NULL;
                    decoder_function = NULL;
                    frame_buffer.song_eof = FALSE;
                }
            } else {
                frame_buffer.pcm_size += new_frame.pcm_size;
            }
            if( frame_buffer.size == 0 ) {
                squash_broadcast( frame_buffer.new_data );
            }
            frame_buffer.frames[frame_buffer.size] = new_frame;
            if( new_frame.pcm_data && new_frame.pcm_size > 0 ) {
                squash_malloc( frame_buffer.frames[frame_buffer.size].pcm_data, new_frame.pcm_size );
                memcpy(frame_buffer.frames[frame_buffer.size].pcm_data, new_frame.pcm_data, new_frame.pcm_size);
            }
            frame_buffer.size++;
        }

        if( frame_buffer.new_file ) {
            frame_buffer.new_file = FALSE;
            decoder_data = frame_buffer.decoder_data;
            decoder_function = frame_buffer.decoder_function;
            close_function = frame_buffer.close_function;
        }

        while( !(frame_buffer.size < PLAYER_MAX_BUFFER_SIZE && frame_buffer.pcm_size < PLAYER_MAX_BUFFER_PCM_SIZE
                    && (decoder_function || frame_buffer.new_file)) ) {
            squash_wait( frame_buffer.restart, frame_buffer.lock );
        }

       squash_unlock( frame_buffer.lock );
    }

    return (void *)NULL;
}

/*
 * Thread start function
 */
void *player( void *input_data ) {
    song_info_t *cur_song;
    sound_device_t *cur_sound_device;
    unsigned int silence_duration;
    sound_format_t sound_format;
    enum { STATE_BEFORE_SONG, STATE_IN_SONG, STATE_AFTER_SONG } play_state;
    player_command_entry_t *command_entry;
    char *full_filename;
    long start_position;

    play_state = STATE_BEFORE_SONG;

    /* make the compiler happy */
    cur_song = NULL;

    while( 1 ) {
        /* Process any commands */
        squash_lock( player_command.lock );
        if( player_command.head != NULL ) {
            squash_unlock( player_command.lock );
            squash_wlock( database_info.lock );
            squash_lock( player_info.lock );
            squash_lock( player_command.lock );
            squash_lock( frame_buffer.lock );
            command_entry = player_command.head;
            while( command_entry != NULL ) {
                squash_log( "Reading command %d", command_entry->command );
                switch( command_entry->command ) {
                    /* TODO: fix this so that skip and stop are not ignored when
                     * not inside a song
                     */
                    case CMD_SKIP:
                        /* Add this and the player will operate more like
                         * a regular CD player:
                        player_info.state = STATE_PLAY;
                         */
                        {
                            int x;
                            for( x = 0; x < frame_buffer.size; x++ ) {
                                squash_free( frame_buffer.frames[x].pcm_data );
                            }
                        }
                        frame_buffer.song_eof = TRUE;
                        frame_buffer.size = 0;
                        frame_buffer.pcm_size = 0;

                        if( play_state == STATE_IN_SONG ) {
                            play_state = STATE_AFTER_SONG;
                        }
                        feedback(cur_song, -1);
                        break;
                    case CMD_STOP:
                        {
                            int x;
                            for( x = 0; x < frame_buffer.size; x++ ) {
                                squash_free( frame_buffer.frames[x].pcm_data );
                            }
                        }
                        player_info.state = STATE_STOP;
                        frame_buffer.size = 0;
                        frame_buffer.pcm_size = 0;
                        squash_broadcast( frame_buffer.restart );

                        if( play_state == STATE_IN_SONG ) {
                            /* Return to the begenning of the song */
                            song_functions[ cur_song->song_type ].seek( frame_buffer.decoder_data, 0, cur_song->play_length );
                            player_info.current_position = 0;

                            /* Reset the spectrum display */
                            spectrum_reset( sound_format );

                            /* Tell the display we've changed */
                            squash_broadcast( display_info.changed );
                        }
                        break;
                    case CMD_PAUSE:
                        player_info.state = STATE_PAUSE;
                        break;
                    case CMD_PLAY:
                        player_info.state = STATE_PLAY;
                        squash_broadcast( frame_buffer.restart );
                        break;
                    default:
                        /* ignore */
                        break;
                }
                command_entry = command_entry->next;
                if( player_command.head == player_command.tail ) {
                    player_command.tail = command_entry;
                }
                squash_free( player_command.head );
                player_command.head = command_entry;
                player_command.size--;
            }
            squash_unlock( player_info.lock );
            squash_unlock( frame_buffer.lock );
            squash_wunlock( database_info.lock );
        }

        /* only pause if we were going to be playing and we shouldn't */
        if( player_info.state == STATE_BIG_STOP ||
            ( play_state == STATE_IN_SONG && player_info.state != STATE_PLAY) ) {
            squash_wait( player_command.changed, player_command.lock );
            squash_unlock( player_command.lock );
            continue;
        }
        squash_unlock( player_command.lock );

        /* Actually play stuff */
        switch( play_state ) {
            case STATE_BEFORE_SONG:
                squash_rlock( database_info.lock );
                squash_lock( song_queue.lock );
                squash_lock( player_info.lock );
                squash_lock( frame_buffer.lock );

                /* Wait for a song to be added */
                while( song_queue.size <= 0 ) {
                    squash_unlock( frame_buffer.lock );
                    squash_unlock( player_info.lock );
                    squash_runlock( database_info.lock );
                    squash_wait( song_queue.not_empty, song_queue.lock );
                    squash_unlock( song_queue.lock );
                    squash_rlock( database_info.lock );
                    squash_lock( song_queue.lock );
                    squash_lock( player_info.lock );
                    squash_lock( frame_buffer.lock );
                }

                /* Get the next song */
                get_next_song_info(&cur_song, &start_position);

                /* Set Now Playing Information */
                set_now_playing_info( cur_song, start_position );

                squash_log("Playing: %s", cur_song->filename);

                squash_unlock( song_queue.lock );

                /* Make sure this is a file we can deal with */
                if( cur_song->song_type == TYPE_UNKNOWN ) {
                    continue;
                }

                /* Open the decoder  */
                squash_asprintf(full_filename, "%s/%s", cur_song->basename[ BASENAME_SONG ], cur_song->filename );

                frame_buffer.decoder_data = song_functions[ cur_song->song_type ].open( full_filename, &sound_format );
                if( frame_buffer.decoder_data == NULL ) {
                    squash_error("Problem opening file: %s");
                }
                squash_free( full_filename );

                /* skip to the start position */
                song_functions[ cur_song->song_type ].seek( frame_buffer.decoder_data, start_position, cur_song->play_length );

                frame_buffer.decoder_function = song_functions[ cur_song->song_type ].decode_frame;
                frame_buffer.close_function = song_functions[ cur_song->song_type ].close;
                frame_buffer.new_file = TRUE;
                {
                    int x;
                    for( x = 0; x < frame_buffer.size; x++ ) {
                        squash_free( frame_buffer.frames[x].pcm_data );
                    }
                }
                frame_buffer.size = 0;
                frame_buffer.pcm_size = 0;
                squash_broadcast( frame_buffer.restart );
                squash_unlock( frame_buffer.lock );
                squash_runlock( database_info.lock );

                silence_duration = 0;

                /* Open the sound device */
                player_info.device = sound_open(sound_format); // mem leak here or at close.
                if( player_info.device == NULL ) {
                    squash_error("Problem opening the sound device!");
                }

                squash_unlock( player_info.lock );

                /* Reset the spectrum for the new song */
                spectrum_reset( sound_format );

                play_state = STATE_IN_SONG;
                break;
            case STATE_IN_SONG:
                squash_lock(frame_buffer.lock);
                if( frame_buffer.size > 0 ) {
                    bool need_more = FALSE;
                    frame_data_t cur_frame;
                    int x;
                    cur_frame = frame_buffer.frames[0];
                    for( x = 1; x < frame_buffer.size; x++ ) {
                        frame_buffer.frames[x - 1] = frame_buffer.frames[x];
                    }
                    frame_buffer.size--;
                    frame_buffer.pcm_size -= cur_frame.pcm_size;
                    if( frame_buffer.size * 10 < PLAYER_MAX_BUFFER_SIZE * 9 || frame_buffer.pcm_size * 10 < PLAYER_MAX_BUFFER_PCM_SIZE * 9 ) {
                        need_more = TRUE;
                    }
                    squash_unlock( frame_buffer.lock );

                    if( cur_frame.pcm_size == 0 ) {
                        /* EOF */
                        squash_wlock( database_info.lock );
                        feedback(cur_song, 1);
                        squash_wunlock( database_info.lock );
                        play_state = STATE_AFTER_SONG;
                    } else if( cur_frame.pcm_size == -1 ) {
                        /* Recoverable error */
                    } else if( cur_frame.pcm_size <= -2 ) {
                        squash_wlock( database_info.lock );
                        feedback(cur_song, 1);
                        squash_wunlock( database_info.lock );
                        play_state = STATE_AFTER_SONG;
                        /* Non-recoverable error */
                    } else {
                        spectrum_update( cur_frame );

                        squash_lock( player_info.lock );
                        /* Signal display, if we haven't updated for a whole second */
                        if( cur_frame.position / 1000 != player_info.current_position / 1000 ) {
                            squash_broadcast( display_info.changed );
                        }
                        player_info.current_position = cur_frame.position;
                        squash_unlock( player_info.lock );

                        cur_sound_device = player_info.device;  /* grab a copy of the device
                                                                 * (hope that the sound driver itself is thread safe */

                        if( !detect_silence(cur_frame, &silence_duration) ) {
                            sound_play( cur_frame, cur_sound_device );
                        }
                        squash_free( cur_frame.pcm_data );
                    }
                    if( need_more ) {
                        squash_broadcast( frame_buffer.restart );
                    }
                } else {
                    squash_broadcast( frame_buffer.restart );
                    squash_wait( frame_buffer.new_data, frame_buffer.lock );
                    squash_unlock( frame_buffer.lock );
                }
                break;
            case STATE_AFTER_SONG:
                squash_rlock( database_info.lock );
                squash_lock( past_queue.lock );
                done_with_song_info( cur_song );
                squash_unlock( past_queue.lock );
                squash_runlock( database_info.lock );

                squash_lock( player_info.lock );
                /* Close the sound device */
                sound_close( player_info.device );
                squash_unlock( player_info.lock );

                play_state = STATE_BEFORE_SONG;
                break;
        }
    }

    return 0;
}

/*
 * Gets the next song off the playlist.
 */
void get_next_song_info( song_info_t **song, long *start_position ) {
    song_queue_entry_t *cur_song_queue_entry;

    if( song_queue.size <= 0 ) {
        *song = NULL;
        *start_position = 0;
        return;
    }

    /* Get a entry from the queue */
    cur_song_queue_entry = song_queue.head;
    if( song_queue.head == song_queue.tail ) {
        song_queue.head = (song_queue_entry_t *)NULL;
        song_queue.tail = (song_queue_entry_t *)NULL;
    } else {
        song_queue.head = song_queue.head->next;
    }
    song_queue.size--;

    /* Signal the playlist_manager thread to add more songs */
    squash_broadcast( song_queue.not_full );

    /* Get the song information */
    *song = cur_song_queue_entry->song_info;
    *start_position = cur_song_queue_entry->start_position;
    squash_free( cur_song_queue_entry );

    /* Set the return information */
}

/*
 * Puts song on the past_queue
 */
void done_with_song_info( song_info_t *song ) {
    song_queue_entry_t *cur_queue_entry, *cur_queue_last_entry;
    int i;

    squash_malloc( cur_queue_entry, sizeof( song_queue_entry_t ) );

    cur_queue_entry->song_info = song;
    cur_queue_entry->next = past_queue.head;

    past_queue.head = cur_queue_entry;
    if( past_queue.tail == NULL ) {
        past_queue.tail = cur_queue_entry;
    }

    past_queue.size++;

    if( past_queue.size > past_queue.wanted_size ) {
        cur_queue_entry = past_queue.head;
        cur_queue_last_entry = NULL;
        for( i = 0; i < past_queue.wanted_size; i++ ) {
            cur_queue_last_entry = cur_queue_entry;
            cur_queue_entry = cur_queue_entry->next;
        }
        past_queue.tail = cur_queue_last_entry;
        past_queue.tail->next = NULL;

        while( cur_queue_entry != NULL ) {
            cur_queue_last_entry = cur_queue_entry;
            cur_queue_entry = cur_queue_entry->next;

            squash_free( cur_queue_last_entry );
        }
        past_queue.size = past_queue.wanted_size;
        if( past_queue.size == 0 ) {
           past_queue.head = NULL;
        }
    }
}

/*
 * This is a fast silence detection hueristic.  I am not a sound engineer.
 * This routine was roughly tuned and appears to work well.  It looks for
 * 30% zeros in a frame.  If this happens for a whole second, then that
 * frame and any following frames are considered silent; until a frame with
 * less than 30% zeros is found.
 * Roughly tuned means on a handful of tracks at 44100hz, stereo and reasonable
 * compression.
 */
int detect_silence( frame_data_t frame_data, unsigned int *silence_duration ) {
    const int one_second = 176400;
    const int threshold = frame_data.pcm_size * 30 / 100; /* 30% of the buffer length */
    int zero_count = 0;
    int i;

    /* Find Zeroes */
    for( i = 0; i < frame_data.pcm_size; i++ ) {
        if( frame_data.pcm_data[i] == 0 ) {
            zero_count++;
            if( zero_count >= threshold ) {
                *silence_duration += frame_data.pcm_size;
                break;
            }
        } else if( (threshold - zero_count) >= (frame_data.pcm_size - i) ) {
            /* there is less than 30% zeros, therefore we are not silent */
            *silence_duration = 0;
            break;
        }
    }

    /* Detemine Silence */
    if( *silence_duration > one_second ) {
        return 1;
    }

    return 0;
}

/*
 * Informs display.c about the now_playing window.
 */
void set_now_playing_info( song_info_t *song, long start_position ) {
    /* Set song */
    player_info.song = song;
    player_info.current_position = start_position;

    /* Signal anybody interested in status */
    squash_broadcast( display_info.changed );
}

/*
 * Called by other commands to queue a command for the player thread
 */
void player_queue_command( enum player_command_e command ) {
    player_command_entry_t *new_entry;

    #ifdef INPUT_DEBUG
    squash_log( "Queueing player_command %d", command );
    #endif

    squash_malloc( new_entry, sizeof(player_command_entry_t) );
    new_entry->command = command;
    new_entry->next = NULL;
    if( player_command.head == NULL ) {
        player_command.head = new_entry;
        player_command.tail = new_entry;
    } else {
        player_command.tail->next = new_entry;
        if( player_command.tail == player_command.head ) {
            player_command.head = new_entry;
        }
    }
    player_command.size++;
}
