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
 * spectrum.h
 */
#ifndef SQUASH_SPECTRUM_H
#define SQUASH_SPECTRUM_H

#define SPECTRUM_WINDOW_SIZE 4096
#define SPECTRUM_RING_SIZE ((SPECTRUM_WINDOW_SIZE) + 1)

#define SPECTRUM_KEEP_AMOUNT 0.30

#define SPECTRUM_MAX_DB 8
#define SPECTRUM_MIN_DB -30

#define SPECTRUM_BAR_RISE_OLD 0.2
#define SPECTRUM_BAR_RISE_NEW 0.8
#define SPECTRUM_BAR_FALL_OLD 0.8
#define SPECTRUM_BAR_FALL_NEW 0.2

#define SPECTRUM_BAR_CAP_FALL_RATE (1.0/60)

#define SPECTRUM_EXP_SCALE 8.0

#define SPECTRUM_BAR_FILL_LEFT ACS_CKBOARD
#define SPECTRUM_BAR_FILL_RIGHT ACS_CKBOARD
#define SPECTRUM_BAR_CAP_LEFT '_'
#define SPECTRUM_BAR_CAP_RIGHT '_'

/*
 * Prototypes
 */
void spectrum_resize(int height, int width);
void *spectrum_monitor( void *input_data );
void spectrum_init( void );
void spectrum_deinit( void );
void spectrum_reset( sound_format_t sound_format );
void spectrum_update( frame_data_t frame );
void spectrum_prepare_render( void );
void spectrum_render( void );

#endif
