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
#ifndef SQUASH_PLAY_OGG_H
#define SQUASH_PLAY_OGG_H

#include <vorbis/codec.h>        /* Vorbis Decoder */
#ifdef TREMOR
    #include <tremor/ivorbisfile.h>    /* Tremor Decoder */
#else
    #include <vorbis/vorbisfile.h>    /* Vorbis Decoder */
#endif

/*
 * Definitions
 */
#define PLAY_OGG_PCM_BUFFER_SIZE 8192

/*
 * Structures
 */
typedef struct ogg_data_s {
    OggVorbis_File file;
    char pcm_data[ PLAY_OGG_PCM_BUFFER_SIZE ];
    long duration;
} ogg_data_t;

/*
 * Prototypes
 */
void *ogg_open( char *filename, sound_format_t *sound_format );
frame_data_t ogg_decode_frame( void *data );
long ogg_calc_duration( void *data );
void ogg_seek( void *data, long seek_time );
void ogg_close( void *data );

#endif
