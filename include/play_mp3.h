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
 * play_mp3.h
 */
#ifndef SQUASH_PLAY_MP3_H
#define SQUASH_PLAY_MP3_H

#include <mad.h>    /* MP3 Decoder */
#undef bool         /* make id3lib play with ncurses, let's hope that doesn't
                     * come back and haunt us. */
#include <id3.h>    /* id3lib to read tags */

/*
 * Structures
 */
typedef struct mp3_data_s {
    int file_size;
    char *buffer;
    FILE *file;
    struct mad_stream stream;
    struct mad_frame frame;
    struct mad_synth synth;
    mad_timer_t timer;
    long duration;
    char *pcm_data;
} mp3_data_t;

/*
 * Prototypes
 */
void *mp3_open( char *filename, sound_format_t *sound_format );
void mp3_load_meta( void *data, char *filename );
frame_data_t mp3_decode_frame( void *data );
long mp3_calc_duration( void *data );
void mp3_seek( void *data, long seek_time );
void mp3_close( void *data );
unsigned int mad_to_16bit( mad_fixed_t f );

#endif
