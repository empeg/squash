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
 * sound.h
 */
#ifndef SQUASH_SOUND_H
#define SQUASH_SOUND_H

/*
 * Prototypes
 */
void sound_init( void );
sound_device_t *sound_open( sound_format_t sound_format );
void sound_set_volume( sound_device_t *sound, int value );
void sound_adjust_volume( sound_device_t *sound, int adjustment );
void sound_play( frame_data_t frame_data, sound_device_t *sound_device );
void sound_close( sound_device_t *device );
void sound_shutdown( void );

#endif
