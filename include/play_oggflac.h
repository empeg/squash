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
#ifndef SQUASH_PLAY_OGGFLAC_H
#define SQUASH_PLAY_OGGFLAC_H

#include <FLAC/all.h>

/*
 * Definitions
 */

/*
 * Structures
 */
typedef struct oggflac_data_s {
    OggFLAC__StreamDecoder *decoder;
    char *buffer;
    int buffer_size;
    int channels;
    int sample_rate;
    long position;
    long duration;
} flac_data_t;

/*
 * Prototypes
 */
void *oggflac_open( char *filename, sound_format_t *sound_format );
OggFLAC__StreamDecoderWriteStatus oggflac_write_callback_load_meta( const OggFLAC__StreamDecoder *decoder, const OggFLAC__Frame *frame, const OggFLAC__int32 * const buffer[], void *client_data );
void oggflac_metadata_callback_load_meta( const OggFLAC__StreamDecoder *decoder, const OggFLAC__StreamMetadata *metadata, void *client_data );
OggFLAC__StreamDecoderWriteStatus oggflac_write_callback_decode_frame( const OggFLAC__StreamDecoder *decoder, const OggFLAC__Frame *frame, const OggFLAC__int32 * const buffer[], void *client_data );
void oggflac_metadata_callback_decode_frame( const OggFLAC__StreamDecoder *decoder, const OggFLAC__StreamMetadata *metadata, void *client_data );
void oggflac_load_meta( void *data, char *filename );
frame_data_t oggflac_decode_frame( void *data );
long oggflac_calc_duration( void *data );
void oggflac_seek( void *data, long seek_time, long duration );
void oggflac_close( void *data );

#endif
