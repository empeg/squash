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
 * play_mp3.c
 */

#include "global.h"
#include "play_mp3.h"

/*
 * Open an mp3 file
 * sound_format will be modified and private state information
 * will be returned.
 */
void *mp3_open( char *filename, sound_format_t *sound_format ) {
    mp3_data_t *mp3_data;
    struct stat file_stat;
    struct mad_header m_header;

    /* Allocate space for data */
    squash_malloc( mp3_data, sizeof(mp3_data_t) );

    /* Open the song */
    if( (mp3_data->file = fopen(filename, "r")) == NULL ) {
        squash_error( "Unable to open file" );
    }

    /* Open the mp3 file */
    fstat(fileno(mp3_data->file), &file_stat);
    mp3_data->file_size = file_stat.st_size;
    mp3_data->buffer = mmap(0, mp3_data->file_size, PROT_READ, MAP_SHARED, fileno(mp3_data->file), 0);
    if( mp3_data->buffer == (void *)-1 ) {
        squash_error( "Unable to open MP3 file" );
    }

    mp3_data->pcm_data = NULL;
    mad_stream_init(&mp3_data->stream);
    mad_frame_init(&mp3_data->frame);
    mad_synth_init(&mp3_data->synth);
    mad_timer_reset(&mp3_data->timer);
    mad_stream_buffer(&mp3_data->stream, (unsigned char *)mp3_data->buffer, mp3_data->file_size);

    while( 1 ) {
        if( mad_header_decode(&m_header, &mp3_data->stream) == -1 ) {
            if( MAD_RECOVERABLE(mp3_data->stream.error) ) {
                continue;
            } else if( mp3_data->stream.error == MAD_ERROR_BUFLEN ) {
                squash_error("Unable to get mp3info (EOF)");
            } else {
                squash_error("Unable to get mp3info (Unrecoverable Error)");
            }
        }
        break;
    }

    sound_format->channels = MAD_NCHANNELS(&m_header);
    sound_format->rate = m_header.samplerate;
    sound_format->byte_format = AO_FMT_LITTLE;
    sound_format->bits = 16;

    mad_header_finish(&m_header);
    mad_stream_finish(&mp3_data->stream);

    mad_stream_init(&mp3_data->stream);
    mad_stream_buffer(&mp3_data->stream, (unsigned char *)mp3_data->buffer, mp3_data->file_size);

    /* Return data */
    return (void *)mp3_data;
}

/*
 * Set a song's metadata based on id3v2 tag,
 */
void mp3_load_meta( void *data, char *filename ) {
    /* TODO: do this */
}

/*
 * Decode a frame of mp3 data.
 */
frame_data_t mp3_decode_frame( void *data ) {
    mp3_data_t *mp3_data = (mp3_data_t *)data;
    frame_data_t frame_data;
    int i, x;
    int result;
    int channels;
    int pcm_size;
    unsigned int sample;

    /* Decode the next frame */
    result = mad_frame_decode(&mp3_data->frame, &mp3_data->stream);
    if (result != 0) {
        if (mp3_data->stream.error == MAD_ERROR_BUFLEN) {
            pcm_size = 0; /* EOF */
        } else if (MAD_RECOVERABLE(mp3_data->stream.error) ) {
            pcm_size = -1; /* Recoverable stream error */
        } else {
            pcm_size = -2; /* Unrecoverable stream error */
        }
    } else {
        mad_timer_add(&mp3_data->timer, mp3_data->frame.header.duration);
        mad_synth_frame(&mp3_data->synth, &mp3_data->frame);
        channels = MAD_NCHANNELS(&mp3_data->frame.header);
        pcm_size = mp3_data->synth.pcm.length*2*channels;
        squash_realloc( mp3_data->pcm_data, sizeof(char)*pcm_size );

        x = 0;
        for(i = 0; i < mp3_data->synth.pcm.length; i++) {
            sample = mad_to_16bit(mp3_data->synth.pcm.samples[0][i]);
            mp3_data->pcm_data[x++] = sample & 0xff;
            mp3_data->pcm_data[x++] = sample >> 8;

            if (channels > 1) {
                sample = mad_to_16bit(mp3_data->synth.pcm.samples[1][i]);
                mp3_data->pcm_data[x++] = sample & 0xff;
                mp3_data->pcm_data[x++] = sample >> 8;
            }
        }
    }
    frame_data.pcm_size = pcm_size;
    frame_data.position = mad_timer_count(mp3_data->timer, MAD_UNITS_MILLISECONDS);
    if( frame_data.position > mp3_data->duration ) {
        frame_data.position = mp3_data->duration;
    }

    /* Setup PCM buffer */
    if( frame_data.pcm_size <= 0 ) {
        frame_data.pcm_data = NULL;
    } else {
        frame_data.pcm_data = mp3_data->pcm_data;
    }

    /* Return the frame data */
    return frame_data;
}

/*
 * Seek to the seek_time position (in milliseconds) in
 * the opened song
 */
void mp3_seek( void *data, long seek_time ) {
    mp3_data_t *mp3_data = (mp3_data_t *)data;
    long new_file_position = (long)((double)mp3_data->file_size * seek_time / mp3_data->duration);
    mad_stream_buffer(&mp3_data->stream, (unsigned char *)&mp3_data->buffer[new_file_position], mp3_data->file_size - new_file_position);
    mad_timer_set( &mp3_data->timer, 0, seek_time, 1000 );
    mad_frame_mute( &mp3_data->frame );
    mad_synth_mute( &mp3_data->synth );
    mp3_data->stream.error = MAD_ERROR_NONE;
    mp3_data->stream.sync = 0;
    return;
}

/*
 * Close the opened song.
 */
void mp3_close( void *data ) {
    mp3_data_t *mp3_data = (mp3_data_t *)data;

    mad_synth_finish(&mp3_data->synth);
    mad_frame_finish(&mp3_data->frame);
    mad_stream_finish(&mp3_data->stream);

    if( munmap(mp3_data->buffer, mp3_data->file_size) ) {
        squash_error( "Unable to munmap" );
    }

    /* Close file */
    fclose( mp3_data->file );

    /* Free allocated storage */
    if ( mp3_data->pcm_data != NULL ) {
        free( mp3_data->pcm_data );
    }
    free( mp3_data );

    return;
}

/*
 * Takes the 24 bit sample given by libmad
 * and return a 16 bit sample.
 */
unsigned int mad_to_16bit(mad_fixed_t f) {
    f += (1L << (MAD_F_FRACBITS - 16));
    if (f >= MAD_F_ONE) {
        f = MAD_F_ONE - 1;
    } else if (f < -MAD_F_ONE) {
        f = -MAD_F_ONE;
    }
    return f >> (MAD_F_FRACBITS + 1 - 16);
}

/*
 * Return the number of milliseconds in the opened song
 */
long mp3_calc_duration(void * data) {
    mp3_data_t *mp3_data = (mp3_data_t *)data;
    struct mad_header m_header;

    while(1) {
        if( mad_header_decode(&m_header, &mp3_data->stream) == -1 ) {
            if( MAD_RECOVERABLE(mp3_data->stream.error) ) {
                continue;
            } else if( mp3_data->stream.error == MAD_ERROR_BUFLEN ) {
                break; /* EOF */
            } else {
                break; /* BAD ERROR, oh well */
            }
        }
        mad_timer_add(&mp3_data->timer, m_header.duration);
    }

    mp3_data->duration = mad_timer_count(mp3_data->timer, MAD_UNITS_MILLISECONDS);

    mad_header_finish(&m_header);
    mad_stream_finish(&mp3_data->stream);

    mad_stream_init(&mp3_data->stream);
    mad_timer_reset(&mp3_data->timer);
    mad_stream_buffer(&mp3_data->stream, (unsigned char *)mp3_data->buffer, mp3_data->file_size);

    return mp3_data->duration;
}
