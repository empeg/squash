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
 * play_flac.h
 */

#include "global.h"
#include "database.h" /* for insert_meta_data */
#include "play_flac.h"

void flac_error_callback(const FLAC__FileDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {
    /* errors?  we don't need no stinking errors */
    return;
}

/*
 * Open an flac file
 * sound_format will be modified and private state information
 * will be returned.
 */
void *flac_open( char *filename, sound_format_t *sound_format ) {
    flac_data_t *flac_data;
    FLAC__FileDecoderState state;

    /* Allocate space for data */
    squash_malloc( flac_data, sizeof(flac_data_t) );

    if( (flac_data->decoder = FLAC__file_decoder_new()) == NULL ) {
        squash_free( flac_data );
        squash_error( "Unable to create flac decoder" );
    }

    if( !FLAC__file_decoder_set_filename( flac_data->decoder, filename ) ) {
        squash_free( flac_data );
        squash_error( "Unable to set filename in decoder" );
    }

    FLAC__file_decoder_set_metadata_callback( flac_data->decoder, flac_metadata_callback_decode_frame );

    FLAC__file_decoder_set_write_callback( flac_data->decoder, flac_write_callback_decode_frame );

    FLAC__file_decoder_set_error_callback( flac_data->decoder, flac_error_callback );

    FLAC__file_decoder_set_client_data( flac_data->decoder, flac_data );

    state = FLAC__file_decoder_init( flac_data->decoder );
    switch( state ) {
        case FLAC__FILE_DECODER_OK:
            /* no problem */
            break;
        case FLAC__FILE_DECODER_END_OF_FILE:
        case FLAC__FILE_DECODER_ERROR_OPENING_FILE:
        case FLAC__FILE_DECODER_MEMORY_ALLOCATION_ERROR:
        case FLAC__FILE_DECODER_SEEK_ERROR:
        case FLAC__FILE_DECODER_SEEKABLE_STREAM_DECODER_ERROR:
        case FLAC__FILE_DECODER_ALREADY_INITIALIZED:
        case FLAC__FILE_DECODER_INVALID_CALLBACK:
        case FLAC__FILE_DECODER_UNINITIALIZED:
            squash_free( flac_data );
            squash_error( "Unable to initialize decoder: %s", FLAC__FileDecoderStateString[ state ] );
            break;
    }

    flac_data->buffer = NULL;
    flac_data->buffer_size = 0;
    flac_data->channels = -1;
    flac_data->sample_rate = -1;
    flac_data->duration = -1;

    FLAC__file_decoder_process_until_end_of_metadata( flac_data->decoder );

    sound_format->rate = flac_data->sample_rate;
    sound_format->channels = flac_data->channels;
    sound_format->bits = 16;
    sound_format->byte_format = SOUND_LITTLE;

    /* Return data */
    return (void *)flac_data;
}

FLAC__StreamDecoderWriteStatus flac_write_callback_load_meta( const FLAC__FileDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data ) {
    /* do nothing ignore any decoded frames (when just loading meta data)*/
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void flac_metadata_callback_load_meta( const FLAC__FileDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data ) {
    FLAC__StreamMetadata_VorbisComment comment = metadata->data.vorbis_comment;
    int i;
    char *start, *end, *key, *value;

    if( metadata->type != FLAC__METADATA_TYPE_VORBIS_COMMENT ) {
        return;
    }

    for(i = 0; i < comment.num_comments; i++ ) {
        /* TODO: Make this faster, don't malloc start. */
        int length = comment.comments[i].length;
        squash_malloc( start, length+1 );
        memcpy( start, comment.comments[i].entry, length );
        start[ length ] = '\0';
        end = strchr( start, '=' );
        if( end == NULL ) {
            continue;
        }
        key = copy_string( start, end - 1 );
        value = strdup( end+1 );
        insert_meta_data( client_data, NULL, key, value );
        squash_free(start);
    }
}

FLAC__StreamDecoderWriteStatus flac_write_callback_decode_frame( const FLAC__FileDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data ) {
    flac_data_t *flac_data = (flac_data_t *)client_data;
    int i, j, k;

    switch( frame->header.number_type ) {
        case FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER:
            squash_error("I don't know how to count in frames");
            break;
        case FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER:
            flac_data->position = frame->header.number.sample_number * 1000 / flac_data->sample_rate;
            break;
    }

    if( flac_data->buffer == NULL || frame->header.blocksize * flac_data->channels * 2 != flac_data->buffer_size ) {
        flac_data->buffer_size = frame->header.blocksize * flac_data->channels * 2;
        squash_realloc( flac_data->buffer, flac_data->buffer_size );
    }

    k = 0;
    for( j = 0; j < frame->header.blocksize; j++ ) {
        for( i = 0; i < flac_data->channels; i++ ) {
            int sample;
            sample = buffer[i][j];
            flac_data->buffer[k++] = sample && 0xFF;
            flac_data->buffer[k++] = sample >> 8;
        }
    }
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void flac_metadata_callback_decode_frame( const FLAC__FileDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data ) {
    flac_data_t *flac_data = (flac_data_t *)client_data;

    if( metadata->type != FLAC__METADATA_TYPE_STREAMINFO ) {
        return;
    }

    /* need to only get channels, sample rate and duration */
    flac_data->channels = metadata->data.stream_info.channels;
    flac_data->sample_rate = metadata->data.stream_info.sample_rate;
    flac_data->duration = metadata->data.stream_info.total_samples * 1000 / flac_data->sample_rate;
}

void flac_load_meta( void *data, char *filename ) {
    FLAC__FileDecoder *decoder;
    FLAC__FileDecoderState state;

    if( (decoder = FLAC__file_decoder_new()) == NULL ) {
        squash_error( "Unable to create flac decoder" );
    }

    if( !FLAC__file_decoder_set_filename( decoder, filename ) ) {
        squash_error( "Unable to set filename in decoder" );
    }

    FLAC__file_decoder_set_metadata_callback( decoder, flac_metadata_callback_load_meta );
    FLAC__file_decoder_set_metadata_respond_all( decoder );

    FLAC__file_decoder_set_write_callback( decoder, flac_write_callback_load_meta );

    FLAC__file_decoder_set_error_callback( decoder, flac_error_callback );

    FLAC__file_decoder_set_client_data( decoder, data );

    state = FLAC__file_decoder_init( decoder );
    switch( state ) {
        case FLAC__FILE_DECODER_OK:
            /* no problem */
            break;
        case FLAC__FILE_DECODER_END_OF_FILE:
        case FLAC__FILE_DECODER_ERROR_OPENING_FILE:
        case FLAC__FILE_DECODER_MEMORY_ALLOCATION_ERROR:
        case FLAC__FILE_DECODER_SEEK_ERROR:
        case FLAC__FILE_DECODER_SEEKABLE_STREAM_DECODER_ERROR:
        case FLAC__FILE_DECODER_ALREADY_INITIALIZED:
        case FLAC__FILE_DECODER_INVALID_CALLBACK:
        case FLAC__FILE_DECODER_UNINITIALIZED:
            squash_error( "Unable to initialize decoder: %s", FLAC__FileDecoderStateString[ state ] );
            break;
    }

    FLAC__file_decoder_process_until_end_of_metadata( decoder );

    FLAC__file_decoder_finish( decoder );
    FLAC__file_decoder_delete( decoder );

    return;
}

/*
 * Decode a frame of ogg data.
 */
frame_data_t flac_decode_frame( void *data ) {
    flac_data_t *flac_data = (flac_data_t *)data;
    frame_data_t frame_data;
    FLAC__FileDecoderState state;

    FLAC__file_decoder_process_single( flac_data->decoder );
    frame_data.position = flac_data->position;

    state = FLAC__file_decoder_get_state( flac_data->decoder );
    switch( state ) {
        case FLAC__FILE_DECODER_OK:
            frame_data.pcm_data = flac_data->buffer;
            frame_data.pcm_size = flac_data->buffer_size;
            break;
        case FLAC__FILE_DECODER_END_OF_FILE:
            frame_data.pcm_data = NULL;
            frame_data.pcm_size = 0;
            break;
        case FLAC__FILE_DECODER_ERROR_OPENING_FILE:
        case FLAC__FILE_DECODER_MEMORY_ALLOCATION_ERROR:
        case FLAC__FILE_DECODER_SEEK_ERROR:
        case FLAC__FILE_DECODER_SEEKABLE_STREAM_DECODER_ERROR:
        case FLAC__FILE_DECODER_ALREADY_INITIALIZED:
        case FLAC__FILE_DECODER_INVALID_CALLBACK:
        case FLAC__FILE_DECODER_UNINITIALIZED:
            squash_error("Error while decoding: %s", FLAC__FileDecoderStateString[ state ] );
            break;
    }

    return frame_data;
}

/*
 * Return the number of milliseconds in the opened song
 */
long flac_calc_duration( void *data ) {
    flac_data_t *flac_data = (flac_data_t *)data;
    return flac_data->duration;
}

/*
 * Seek to the seek_time position (in milliseconds) in
 * the opened song
 */
void flac_seek( void *data, long seek_time, long duration ) {
    flac_data_t *flac_data = (flac_data_t *)data;

    FLAC__file_decoder_seek_absolute( flac_data->decoder, seek_time * (flac_data->sample_rate / 1000) );
    return;
}

/*
 * Close the opened song.
 */
void flac_close( void *data ) {
    flac_data_t *flac_data = (flac_data_t *)data;

    FLAC__file_decoder_finish( flac_data->decoder );

    FLAC__file_decoder_delete( flac_data->decoder );

    /* Free allocated storage */
    squash_free( flac_data->buffer );
    flac_data->buffer_size = 0;
    squash_free( flac_data );

    return;
}
