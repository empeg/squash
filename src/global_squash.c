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
 * global_squash.c
 * Global Functions (For squash only)
 */

#include "global.h"
#include "global_squash.h"
#include "playlist_manager.h" /* for playlist_queue_song() */
#include "display.h"
#ifdef EMPEG_DSP
#include <sys/ioctl.h>          /* for ioctl() */
#include "sys/soundcard.h"      /* for _SIO*() macros */
#endif
#include "database.h"

/* Used by parse_file() to set state values read */
void load_state_callback( void *data, char *header, char *key, char *value ) {
    state_info_t *pass = data;

    squash_ensure_alloc( pass->raw_song_count, pass->raw_song_alloc, pass->raw_songs, sizeof(state_raw_song_t), 10, *=2 );

    /* Add it to the structure */
    if( strncasecmp("entry_filename", key, 16) == 0 ) {
        pass->raw_songs[pass->raw_song_count].filename = value;
        pass->raw_songs[pass->raw_song_count].position = 0;
    } else if( strncasecmp("entry_position", key, 16) == 0 ) {
        pass->raw_songs[pass->raw_song_count].position = atol(value);
        pass->raw_song_count++;
        squash_free( value );
    } else if( strncasecmp("current_entry", key, 15) == 0 ) {
        pass->current_song = atol(value);
        squash_free( value );
    }
#ifdef EMPEG_DSP
    else if( strncasecmp("volume", key, 7) == 0 ) {
        pass->volume = atoi(value);
        squash_free( value );
    }
#endif
#ifdef EMPEG
    else if( strncasecmp("brightness", key, 11) == 0 ) {
        pass->brightness = atoi(value);
        squash_free( value );
    }
#endif
#ifndef NO_NCURSES
    else if( strncasecmp("window_number", key, 14) == 0 ) {
        pass->cur_window_number = atoi(value);
        squash_free( value );
    } else if ( strncasecmp("window_state", key, 13) == 0 ) {
        pass->window_states[pass->cur_window_number] = atoi(value);
        squash_free( value );
    }
#endif
    else {
        squash_free( value );
    }

    squash_free( key );
}

/*
 * Load state
 *
 */
void load_state() {
    song_info_t *song;
    int i;

    squash_lock( state_info.lock );
    state_info.raw_songs = NULL;
    state_info.current_song = 0;
    state_info.raw_song_count = 0;
    state_info.raw_song_alloc = 0;
    state_info.cur_window_number = 0;
    state_info.volume = 50;
    state_info.brightness = 100;
#ifndef NO_NCURSES
    for( i = 0; i < WIN_COUNT; i++ ) {
        state_info.window_states[i] = WIN_STATE_NORMAL;
    }
#endif

    parse_file( config.global_state_path, load_state_callback, &state_info );

#ifdef EMPEG_DSP
    {
        int mixer;
        int raw_vol = state_info.volume + (state_info.volume << 8);
        if ((mixer = open("/dev/mixer", O_RDWR)) == -1) {
            fprintf(stderr, "Can't open /dev/mixer");
            exit(1);
        }
        ioctl( mixer, _SIOWR('M', 0, int), &raw_vol );
    }
#endif

#ifdef EMPEG
    squash_lock( display_info.lock );
    display_info.brightness = state_info.brightness;
    set_display_brightness_empeg( display_info.brightness );
    squash_unlock( display_info.lock );
#endif

    squash_unlock( state_info.lock );


#ifndef NO_NCURSES
    squash_lock( state_info.lock );
    squash_lock( display_info.lock );
    for( i = 0; i < WIN_COUNT; i++ ) {
        display_info.window[i].state = state_info.window_states[i];
    }
    squash_unlock( display_info.lock );
    squash_unlock( state_info.lock );
#endif

    squash_lock(state_info.lock);
    while( state_info.current_song < state_info.raw_song_count ) {
        squash_wlock(database_info.lock);
        squash_lock(song_queue.lock);
        squash_log("adding: %d, %s", state_info.raw_songs[state_info.current_song].position, state_info.raw_songs[state_info.current_song].filename);
        song = find_song_by_filename(state_info.raw_songs[state_info.current_song].filename);
        if( song != NULL ) {
            playlist_queue_song( song, state_info.raw_songs[state_info.current_song].position );
        }
        state_info.current_song++;
        squash_wunlock(database_info.lock);
        squash_unlock(song_queue.lock);
        squash_unlock(state_info.lock);
        squash_broadcast( song_queue.not_empty );
        squash_broadcast( display_info.changed );
        sched_yield();
        squash_lock(state_info.lock);
    }

    for( i = 0; i < state_info.raw_song_count; i++ ) {
        squash_free( state_info.raw_songs[i].filename );
    }
    squash_free( state_info.raw_songs );

    state_info.current_song = 0;
    state_info.raw_song_count = 0;
    state_info.raw_song_alloc = 0;
    squash_unlock( state_info.lock );
}


