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
 * play_ogg.h
 */

#include "global.h"
#include "database.h" /* for insert_meta_data */
#include "play_ogg.h"

/*
 * Open an ogg file
 * sound_format will be modified and private state information
 * will be returned.
 */
void *ogg_open( char *filename, sound_format_t *sound_format ) {
    ogg_data_t *ogg_data;
    vorbis_info *vorbis_info;
    FILE *file;

    /* Open the song */
    if( (file = fopen(filename, "r")) == NULL ) {
        squash_error( "Unable to open file" );
    }

    /* Allocate space for data */
    squash_malloc( ogg_data, sizeof(ogg_data_t) );

    /* Open the vorbis file */
    if( ov_open(file, &ogg_data->file, NULL, 0) != 0 ) {
        squash_free( ogg_data );
        fclose( file );
        squash_error( "Unable to open Vorbis file" );
    }

    vorbis_info = ov_info(&ogg_data->file, -1);
    sound_format->rate = vorbis_info->rate;
    sound_format->channels = vorbis_info->channels;
    sound_format->bits = 16;
    sound_format->byte_format = SOUND_LITTLE;

    /* Return data */
    return (void *)ogg_data;
}

/*
 * Set a song's metadata based on vorbis comments,
 */
void ogg_load_meta( void *data, char *filename ) {
#ifdef NO_VORBIS_COMMENT
#else
    OggVorbis_File ogg_file;
    vorbis_comment *comment;
    FILE *file;
    char *start, *end, *key, *value;
    int i;

    /* Open the song */
    if( (file = fopen(filename, "r")) == NULL ) {
        squash_error( "Unable to open file %s", filename );
    }

    /* Open the vorbis file */
    if( ov_test(file, &ogg_file, NULL, 0) != 0 ) {
        fclose( file );
        return;
    }

    comment = ov_comment( &ogg_file, -1 );

    if( comment == NULL ) {
        ov_clear( &ogg_file );
        return;
    }

    for(i = 0; i < comment->comments; i++ ) {
        start = comment->user_comments[i];
        end = strchr( start, '=' );
        if( end == NULL ) {
            continue;
        }
        key = copy_string( start, end - 1 );
        value = strdup( end+1 );
        insert_meta_data( data, NULL, key, value );
    }

    ov_clear( &ogg_file );
#endif
}

/*
 * Decode a frame of ogg data.
 */
frame_data_t ogg_decode_frame( void *data ) {
    ogg_data_t *ogg_data = (ogg_data_t *)data;
    frame_data_t frame_data;
    int song_section;

    /* Decode the next frame */
#ifdef TREMOR
    long cur_time;
    frame_data.pcm_size = ov_read( &ogg_data->file, ogg_data->pcm_data, PLAY_OGG_PCM_BUFFER_SIZE, &song_section );
    cur_time = ov_time_tell( &ogg_data->file );
    frame_data.position = cur_time;
#else
    double cur_time;
    frame_data.pcm_size = ov_read( &ogg_data->file, ogg_data->pcm_data, PLAY_OGG_PCM_BUFFER_SIZE, 0, 2, 1, &song_section );
    cur_time = ov_time_tell( &ogg_data->file );
    frame_data.position = (long)(cur_time * 1000);
#endif

    /* Setup PCM buffer */
    if( frame_data.pcm_size <= 0 ) {
        frame_data.pcm_data = NULL;
    } else {
        frame_data.pcm_data = ogg_data->pcm_data;
    }

    /* Return the frame data */
    return frame_data;
}

/*
 * Return the number of milliseconds in the opened song
 */
long ogg_calc_duration( void *data ) {
    ogg_data_t *ogg_data = (ogg_data_t *)data;
#ifdef TREMOR
    long total_time = ov_time_total( &ogg_data->file, -1 );
    return total_time;
#else
    double total_time = ov_time_total( &ogg_data->file, -1 );
    return (long)( total_time * 1000 );
#endif
}

/*
 * Seek to the seek_time position (in milliseconds) in
 * the opened song
 */
void ogg_seek( void *data, long seek_time ) {
    ogg_data_t *ogg_data = (ogg_data_t *)data;
#ifdef TREMOR
    ov_time_seek( &ogg_data->file, seek_time );
#else
    ov_time_seek( &ogg_data->file, ((double)seek_time) / 1000 );
#endif
    return;
}

/*
 * Close the opened song.
 */
void ogg_close( void *data ) {
    ogg_data_t *ogg_data = (ogg_data_t *)data;

    /* Close vorbis file */
    ov_clear( &ogg_data->file );
    /* closing the vorbis file also closed the FILE*! */

    /* Free allocated storage */
    squash_free( ogg_data );

    return;
}
