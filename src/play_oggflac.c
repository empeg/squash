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
 * play_oggflac.h
 */

/* This is incomplete -- OggFLAC library will eventually support the FileDecoder
 * interface, so I'm not going to bother with the streamdecoder interface, since
 * it's a lot of wasted effort.
 */

#include "global.h"
#include "database.h" /* for insert_meta_data */
#include "play_oggflac.h"

void oggflac_error_callback(const OggFLAC__StreamDecoder *decoder, OggFLAC__StreamDecoderErrorStatus status, void *client_data) {
    /* errors?  we don't need no stinking errors */
    return;
}

/*
 * Open an oggflac file
 * sound_format will be modified and private state information
 * will be returned.
 */
void *oggflac_open( char *filename, sound_format_t *sound_format ) {
    oggflac_data_t *oggflac_data;
    OggFLAC__StreamDecoderState state;

    /* Allocate space for data */
    squash_malloc( oggflac_data, sizeof(oggflac_data_t) );

    if( (oggflac_data->decoder = OggFLAC__stream_decoder_new()) == NULL ) {
        squash_free( oggflac_data );
        squash_error( "Unable to create oggflac decoder" );
    }

    if( !OggFLAC__stream_decoder_set_filename( oggflac_data->decoder, filename ) ) {
        squash_free( oggflac_data );
        squash_error( "Unable to set filename in decoder" );
    }

    OggFLAC__stream_decoder_set_metadata_callback( oggflac_data->decoder, oggflac_metadata_callback_decode_frame );

    OggFLAC__stream_decoder_set_write_callback( oggflac_data->decoder, oggflac_write_callback_decode_frame );

    OggFLAC__stream_decoder_set_error_callback( oggflac_data->decoder, oggflac_error_callback );

    OggFLAC__stream_decoder_set_client_data( oggflac_data->decoder, oggflac_data );

    state = OggFLAC__stream_decoder_init( oggflac_data->decoder );
    switch( state ) {
        case OggFLAC__STREAM_DECODER_OK:
            /* no problem */
            break;
        case OggFLAC__STREAM_DECODER_OGG_ERROR:
        case OggFLAC__STREAM_DECODER_READ_ERROR
        case OggFLAC__STREAM_DECODER_FLAC_STREAM_DECODER_ERROR:
        case OggFLAC__STREAM_DECODER_INVALID_CALLBACK:
        case OggFLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
        case OggFLAC__STREAM_DECODER_ALREADY_INITIALIZED:
        case OggFLAC__STREAM_DECODER_UNINITIALIZED:
            squash_free( oggflac_data );
            squash_error( "Unable to initialize decoder: %s", OggFLAC__StreamDecoderStateString[ state ] );
            break;
    }

    oggflac_data->buffer = NULL;
    oggflac_data->buffer_size = 0;
    oggflac_data->channels = -1;
    oggflac_data->sample_rate = -1;
    oggflac_data->duration = -1;

    OggFLAC__stream_decoder_process_until_end_of_metadata( oggflac_data->decoder );

    sound_format->rate = oggflac_data->sample_rate;
    sound_format->channels = oggflac_data->channels;
    sound_format->bits = 16;
    sound_format->byte_format = SOUND_LITTLE;

    /* Return data */
    return (void *)oggflac_data;
}

OggFLAC__StreamDecoderWriteStatus oggflac_write_callback_load_meta( const OggFLAC__StreamDecoder *decoder, const OggFLAC__Frame *frame, const OggFLAC__int32 * const buffer[], void *client_data ) {
    /* do nothing ignore any decoded frames (when just loading meta data)*/
    return OggFLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void oggflac_metadata_callback_load_meta( const OggFLAC__StreamDecoder *decoder, const OggFLAC__StreamMetadata *metadata, void *client_data ) {
    OggFLAC__StreamMetadata_VorbisComment comment = metadata->data.vorbis_comment;
    int i;
    char *start, *end, *key, *value;

    if( metadata->type != OggFLAC__METADATA_TYPE_VORBIS_COMMENT ) {
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

OggFLAC__StreamDecoderWriteStatus oggflac_write_callback_decode_frame( const OggFLAC__StreamDecoder *decoder, const OggFLAC__Frame *frame, const OggFLAC__int32 * const buffer[], void *client_data ) {
    oggflac_data_t *oggflac_data = (oggflac_data_t *)client_data;
    int i, j, k;

    switch( frame->header.number_type ) {
        case OggFLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER:
            squash_error("I don't know how to count in frames");
            break;
        case OggFLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER:
            oggflac_data->position = frame->header.number.sample_number * 1000 / oggflac_data->sample_rate;
            break;
    }

    if( oggflac_data->buffer == NULL || frame->header.blocksize * oggflac_data->channels * 2 != oggflac_data->buffer_size ) {
        oggflac_data->buffer_size = frame->header.blocksize * oggflac_data->channels * 2;
        squash_realloc( oggflac_data->buffer, oggflac_data->buffer_size );
    }

    k = 0;
    for( j = 0; j < frame->header.blocksize; j++ ) {
        for( i = 0; i < oggflac_data->channels; i++ ) {
            int sample;
            sample = buffer[i][j];
            oggflac_data->buffer[k++] = sample && 0xFF;
            oggflac_data->buffer[k++] = sample >> 8;
        }
    }
    return OggFLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void oggflac_metadata_callback_decode_frame( const OggFLAC__StreamDecoder *decoder, const OggFLAC__StreamMetadata *metadata, void *client_data ) {
    oggflac_data_t *oggflac_data = (oggflac_data_t *)client_data;

    if( metadata->type != OggFLAC__METADATA_TYPE_STREAMINFO ) {
        return;
    }

    /* need to only get channels, sample rate and duration */
    oggflac_data->channels = metadata->data.stream_info.channels;
    oggflac_data->sample_rate = metadata->data.stream_info.sample_rate;
    oggflac_data->duration = metadata->data.stream_info.total_samples * 1000 / oggflac_data->sample_rate;
}

void oggflac_load_meta( void *data, char *filename ) {
    OggFLAC__StreamDecoder *decoder;
    OggFLAC__StreamDecoderState state;

    if( (decoder = OggFLAC__stream_decoder_new()) == NULL ) {
        squash_error( "Unable to create oggflac decoder" );
    }

    if( !OggFLAC__stream_decoder_set_filename( decoder, filename ) ) {
        squash_error( "Unable to set filename in decoder" );
    }

    OggFLAC__stream_decoder_set_metadata_callback( decoder, oggflac_metadata_callback_load_meta );
    OggFLAC__stream_decoder_set_metadata_respond_all( decoder );

    OggFLAC__stream_decoder_set_write_callback( decoder, oggflac_write_callback_load_meta );

    OggFLAC__stream_decoder_set_error_callback( decoder, oggflac_error_callback );

    OggFLAC__stream_decoder_set_client_data( decoder, data );

    state = OggFLAC__stream_decoder_init( decoder );
    switch( state ) {
        case OggFLAC__STREAM_DECODER_OK:
            /* no problem */
            break;
        case OggFLAC__STREAM_DECODER_END_OF_FILE:
        case OggFLAC__STREAM_DECODER_ERROR_OPENING_FILE:
        case OggFLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
        case OggFLAC__STREAM_DECODER_SEEK_ERROR:
        case OggFLAC__STREAM_DECODER_SEEKABLE_STREAM_DECODER_ERROR:
        case OggFLAC__STREAM_DECODER_ALREADY_INITIALIZED:
        case OggFLAC__STREAM_DECODER_INVALID_CALLBACK:
        case OggFLAC__STREAM_DECODER_UNINITIALIZED:
            squash_error( "Unable to initialize decoder: %s", OggFLAC__StreamDecoderStateString[ state ] );
            break;
    }

    OggFLAC__stream_decoder_process_until_end_of_metadata( decoder );

    OggFLAC__stream_decoder_finish( decoder );
    OggFLAC__stream_decoder_delete( decoder );

    return;
}

/*
 * Decode a frame of ogg data.
 */
frame_data_t oggflac_decode_frame( void *data ) {
    oggflac_data_t *oggflac_data = (oggflac_data_t *)data;
    frame_data_t frame_data;
    OggFLAC__StreamDecoderState state;

    OggFLAC__stream_decoder_process_single( oggflac_data->decoder );
    frame_data.position = oggflac_data->position;

    state = OggFLAC__stream_decoder_get_state( oggflac_data->decoder );
    switch( state ) {
        case OggFLAC__STREAM_DECODER_OK:
            frame_data.pcm_data = oggflac_data->buffer;
            frame_data.pcm_size = oggflac_data->buffer_size;
            break;
        case OggFLAC__STREAM_DECODER_END_OF_FILE:
            frame_data.pcm_data = NULL;
            frame_data.pcm_size = -2;
            break;
        case OggFLAC__STREAM_DECODER_ERROR_OPENING_FILE:
        case OggFLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
        case OggFLAC__STREAM_DECODER_SEEK_ERROR:
        case OggFLAC__STREAM_DECODER_SEEKABLE_STREAM_DECODER_ERROR:
        case OggFLAC__STREAM_DECODER_ALREADY_INITIALIZED:
        case OggFLAC__STREAM_DECODER_INVALID_CALLBACK:
        case OggFLAC__STREAM_DECODER_UNINITIALIZED:
            squash_error("Error while decoding: %s", OggFLAC__StreamDecoderStateString[ state ] );
            break;
    }

    return frame_data;
}

/*
 * Return the number of milliseconds in the opened song
 */
long oggflac_calc_duration( void *data ) {
    oggflac_data_t *oggflac_data = (oggflac_data_t *)data;
    return oggflac_data->duration;
}

/*
 * Seek to the seek_time position (in milliseconds) in
 * the opened song
 */
void oggflac_seek( void *data, long seek_time, long duration ) {
    /* not done yet */
    return;
}

/*
 * Close the opened song.
 */
void oggflac_close( void *data ) {
    oggflac_data_t *oggflac_data = (oggflac_data_t *)data;

    OggFLAC__stream_decoder_finish( oggflac_data->decoder );

    OggFLAC__stream_decoder_delete( oggflac_data->decoder );

    /* Free allocated storage */
    squash_free( oggflac_data->buffer );
    oggflac_data->buffer_size = 0;
    squash_free( oggflac_data );

    return;
}
