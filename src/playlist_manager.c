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
 * playlist_manager.c
 */

#include "global.h"
#include "database.h"   /* for save_song() */
#include "stat.h"       /* for pick_song() */
#include "player.h"     /* for song_functions[] */
#include "playlist_manager.h"

#include <sys/time.h>   /* for gettimeofday() */

void ensure_song_fully_loaded( song_info_t *song_info ) {
    if( song_info == NULL ) {
        return;
    }

    if( song_info->song_type == -1 ) {
        squash_log("loading song type");
        song_info->song_type = get_song_type( song_info->basename[ BASENAME_SONG ], song_info->filename );
    }

    if( song_info->meta_key_count == -1 ) {
        squash_log("loading metadata");
        /* Load the meta data if it hasn't been loaded */
        load_meta_data( song_info, TYPE_META );
    }

    if( song_info->play_length == -1 ) {
        /* Load the play length if it hasn't been loaded */
        sound_format_t sound_format;
        void *decoder_data;
        char *full_filename;
        long play_length;
        meta_key_t *meta_length;
        squash_log("loading play length");

        /* See if it's stored in the metadata */

        /* empeg's metainfo files use duration instead of length */
        meta_length = get_meta_data( song_info, "duration" );
        if( meta_length == NULL ) {
            meta_length = get_meta_data( song_info, "length" );
        }
        if( meta_length != NULL &&  meta_length->value_count != 0 ) {
            song_info->play_length = atoi(meta_length->values[0]);
        /* Otherwise, we need to get it from the file, but make sure this is a file we can deal with */
        } else if( song_info->song_type != TYPE_UNKNOWN ) {
            /* Open the decoder  */
            squash_asprintf(full_filename, "%s/%s", song_info->basename[ BASENAME_SONG ], song_info->filename );

            decoder_data = song_functions[ song_info->song_type ].open( full_filename, &sound_format );
            if( decoder_data == NULL ) {
                squash_error("Can't open file %s", full_filename );
            }

            /* Set Now Playing Information */
            play_length = song_functions[ song_info->song_type ].calc_duration( decoder_data );

            song_info->play_length = play_length;

            /* Close the decoder */
            song_functions[ song_info->song_type ].close( decoder_data );

            squash_free( full_filename );
        }
    }
}

/*
 * Function called for the playlist_manager thread
 * This routine ensures that there is always song_queue.wanted_size
 * entries on the playlist.  It uses stat.c's pick_song
 * to choose what songs to add.
 */
void *playlist_manager( void *input_data ) {
    struct timeval cur_time;

    gettimeofday( &cur_time, NULL );
    srand( cur_time.tv_sec );

    /* First wait for the system to be loaded (wanted_size to be set). */
    /* Acquire lock */
    squash_lock( song_queue.lock );

    /* While we have too many songs (in this case, wanted_size has not been set) */
    while( song_queue.size >= song_queue.wanted_size ) {
        squash_wait( song_queue.not_full, song_queue.lock );
    }

    /* Unlock the mutexs */
    squash_unlock( song_queue.lock );

    /* Load songs */
    while( 1 ) {
        squash_log("playlist loop");
        /* Acquire lock */
        squash_wlock( database_info.lock );
        squash_lock( song_queue.lock );

        /* We only get in this loop if there are too many (or just enough) songs */
        while( song_queue.size >= song_queue.wanted_size ) {
            squash_wunlock( database_info.lock );
            squash_wait( song_queue.not_full, song_queue.lock );
            squash_unlock( song_queue.lock );
            squash_wlock( database_info.lock );
            squash_lock( song_queue.lock );
        }

        /* We only get here if we have things to add */

        /* Check to see if there are any songs */
        if( database_info.song_count <= 0 ) {
            squash_error("This shouldn't happen, database is empty!");
        }

        /* Now wait for the statistics data to be loaded */
        while( !database_info.stats_loaded ) {
            squash_log("Waiting for stats to load");
            squash_wunlock( database_info.lock );
            squash_wait( database_info.stats_finished, song_queue.lock );
            squash_unlock( song_queue.lock );
            squash_wlock( database_info.lock );
            squash_lock( song_queue.lock );
        }

        playlist_queue_song( &database_info.songs[ pick_song() ], 0 );

        /* Save the database statistics to disk if we are done adding songs */
        if( song_queue.size >= song_queue.wanted_size ) {
            /* Sync pick_song changes to disk */
            int i;
            for( i = 0; i < database_info.song_count; i++ ) {
                save_song( &database_info.songs[i] );
            }
        }

        /* Unlock the mutexs */
        squash_unlock( song_queue.lock );
        squash_wunlock( database_info.lock );

        /* Signal the player thread waiting for songs */
        squash_broadcast( song_queue.not_empty );
        squash_broadcast( display_info.changed );
    }

    return 0;
}

/*
 * Add an entry to the playlist
 */
void playlist_queue_song( song_info_t *song, long start_position ) {
    song_queue_entry_t *new_song_queue_entry;

    /* ensure that the song is fully loaded (meta info, song length and song type) */
    ensure_song_fully_loaded( song );

    /* Allocate space for a new queue entry */
    squash_malloc( new_song_queue_entry, sizeof(song_queue_entry_t) );

    /* Setup the queue entry */
    new_song_queue_entry->song_info = song;
    new_song_queue_entry->start_position = start_position;
    new_song_queue_entry->next = (song_queue_entry_t *)NULL;

    /* Add to the queue */
    if( song_queue.head == NULL )
        song_queue.head = new_song_queue_entry;
    if( song_queue.tail != NULL )
        song_queue.tail->next = new_song_queue_entry;
    song_queue.tail = new_song_queue_entry;
    song_queue.size++;
    squash_log("song_queue.size = %d", song_queue.size);
}
