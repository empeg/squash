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
 * sound.c
 */

#include "global.h"
#include "sound.h"

/*
 * Initialize sound driver
 */
void sound_init( void ) {
    ao_initialize();
}

/*
 * Open an instance of the sound device.
 * An instance can play sound and multiple, but
 * different instances used at once will mix.
 */
sound_device_t *sound_open( sound_format_t format ) {
    int default_driver;

    default_driver = ao_default_driver_id();

    return ao_open_live( default_driver, &format, NULL );
}

/*
 * Play a frame of data
 */
void sound_play( frame_data_t frame_data, sound_device_t *sound_device ) {
    ao_play( sound_device, frame_data.pcm_data, frame_data.pcm_size );
}

/*
 * Close a sound device instance
 */
void sound_close( sound_device_t *device ) {
    ao_close( device );
}

/*
 * Shutdown the sound driver
 */
void sound_shutdown( void ) {
    ao_shutdown();
}
