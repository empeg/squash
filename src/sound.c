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

#ifdef EMPEG_DSP
#include <sys/types.h> /* for open() */
#include <sys/stat.h> /* for open() */
#include <fcntl.h> /* for open() */
#include <unistd.h> /* for write() */
#include <sys/ioctl.h> /* for ioctl() */
#include "sys/soundcard.h" /* for SOUND_MASK_PCM and _SIO*() macros */
#define EMPEG_AUDIO_DEVICE        "/dev/audio"
#define EMPEG_AUDIO_BUFFER_SIZE   4608
#define EMPEG_MIXER_DEVICE        "/dev/mixer"

#define EMPEG_MIXER_MAGIC 'm'
#define EMPEG_MIXER_WRITE_SOURCE _IOW(EMPEG_MIXER_MAGIC, 0, int)
#define EMPEG_MIXER_WRITE_FLAGS _IOW(EMPEG_MIXER_MAGIC, 1, int)
#define EMPEG_MIXER_SET_SAM _IOW(EMPEG_MIXER_MAGIC, 15, int)

#endif

/*
 * Initialize sound driver
 */
void sound_init( void ) {
#ifdef EMPEG_DSP
#else
    ao_initialize();
#endif
}

/*
 * Open an instance of the sound device.
 * An instance can play sound and multiple, but
 * different instances used at once will mix.
 */
sound_device_t *sound_open( sound_format_t format ) {
#ifdef EMPEG_DSP
    sound_device_t *t;
    int i;
    int raw_vol;
    squash_malloc( t, sizeof(sound_device_t) );

    squash_malloc( t->buffer, EMPEG_AUDIO_BUFFER_SIZE );
    t->buffer_size = 0;

    if ((t->audio = open(EMPEG_AUDIO_DEVICE, O_WRONLY)) == -1) {
        squash_error("Can't open " EMPEG_AUDIO_DEVICE);
    }

    if ((t->mixer = open(EMPEG_MIXER_DEVICE, O_RDWR)) == -1) {
        squash_error("Can't open " EMPEG_MIXER_DEVICE);
    }

    i = SOUND_MASK_PCM;
    if (ioctl(t->mixer, EMPEG_MIXER_WRITE_SOURCE, &i) != 0) {
        squash_error("Can't select mixer->pcm");
    }

    i = 0;
    if (ioctl(t->mixer, EMPEG_MIXER_SET_SAM, &i) != 0) {
        squash_error("Can't turn off soft audio mute");
    }

    i = 0;
    if (ioctl(t->mixer, EMPEG_MIXER_WRITE_FLAGS, &i) != 0) {
        squash_error("Can't turn off mute");
    }

    /* This reads the current volume stored in the mixer. */
    ioctl( t->mixer, _SIOR('M', 0, int), &raw_vol );
    t->volume[0] = raw_vol & 0xFF;
    t->volume[1] = (raw_vol >> 8) & 0xFF;

    return t;
#else
    int default_driver;

    default_driver = ao_default_driver_id();

    return ao_open_live( default_driver, &format, NULL );
#endif
}

void sound_set_volume( sound_device_t *sound, int value ) {
#ifdef EMPEG_DSP
    int raw_vol;
    if( sound != NULL ) {
        if( value > 100 ) {
            value = 100;
        }
        if( value < 0 ) {
            value = 0;
        }
        squash_log("setting volume to %d", value);
        sound->volume[0] = value;
        sound->volume[1] = value;
        raw_vol = sound->volume[0] + (sound->volume[1] << 8);
        ioctl( sound->mixer, _SIOWR('M', 0, int), &raw_vol );
    }
#endif
}

void sound_adjust_volume( sound_device_t *sound, int adjustment ) {
#ifdef EMPEG_DSP
    int raw_vol;
    if( sound != NULL ) {
        sound->volume[0] += adjustment;
        if( sound->volume[0] > 100 ) {
            sound->volume[0] = 100;
        }
        if( sound->volume[0] < 0 ) {
            sound->volume[0] = 0;
        }
        sound->volume[1] += adjustment;
        if( sound->volume[1] > 100 ) {
            sound->volume[1] = 100;
        }
        if( sound->volume[1] < 0 ) {
            sound->volume[1] = 0;
        }
        raw_vol = sound->volume[0] + (sound->volume[1] << 8);
        ioctl( sound->mixer, _SIOWR('M', 0, int), &raw_vol );
    }
#endif
}

/*
 * Play a frame of data
 */
void sound_play( frame_data_t frame_data, sound_device_t *sound_device ) {
#ifdef EMPEG_DSP
    int i, pcm_remain, buffer_remain, copy_amount;
    i = 0;
    while( i < frame_data.pcm_size ) {
        buffer_remain = EMPEG_AUDIO_BUFFER_SIZE - sound_device->buffer_size;
        if( buffer_remain > 0 ) {
            /* there is still space to put some data */
            pcm_remain = frame_data.pcm_size - i;
            copy_amount = pcm_remain > buffer_remain ? buffer_remain : pcm_remain;
            memcpy( &sound_device->buffer[sound_device->buffer_size], &frame_data.pcm_data[i], copy_amount );
            i += copy_amount;
            sound_device->buffer_size += copy_amount;
            buffer_remain -= copy_amount;
        }
        if( buffer_remain == 0 ) {
            /* buffer is full, so play it */
            if( (write( sound_device->audio, sound_device->buffer, EMPEG_AUDIO_BUFFER_SIZE )) == -1 ) {
                squash_error("problem writing to " EMPEG_AUDIO_DEVICE );
            }
            sound_device->buffer_size = 0;
        }
    }
#else
    ao_play( sound_device, frame_data.pcm_data, frame_data.pcm_size );
#endif
}

/*
 * Close a sound device instance
 */
void sound_close( sound_device_t *device ) {
#ifdef EMPEG_DSP
    /*
     * Let's play it safe, if we close and the buffer still has information,
     * we will flush the buffer.  But since we can only write out whole bytes
     * of EMPEG_AUDIO_BUFFER_SIZE, we will fill up any slack with zero's first.
     */
    if( device->buffer_size != 0 ) {
        int buffer_remain;
        buffer_remain = EMPEG_AUDIO_BUFFER_SIZE - device->buffer_size;
        memset( &device->buffer[device->buffer_size], 0, buffer_remain );
        if( (write( device->audio, device->buffer, EMPEG_AUDIO_BUFFER_SIZE )) == -1 ) {
           squash_error("problem writing to " EMPEG_AUDIO_DEVICE );
        }
        device->buffer_size = 0;
    }
    close( device->audio );
    close( device->mixer );
    squash_free( device->buffer );
    squash_free( device );
#else
    ao_close( device );
#endif
}

/*
 * Shutdown the sound driver
 */
void sound_shutdown( void ) {
#ifdef EMPEG_DSP
#else
    ao_shutdown();
#endif
}
