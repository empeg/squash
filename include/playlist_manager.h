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
 * playlist_manager.h
 */
#ifndef SQUASH_PLAYLIST_MANAGER_H
#define SQUASH_PLAYLIST_MANAGER_H

#include <sys/time.h>   /* for gettimeofday() */

/*
 * Prototypes
 */
void ensure_song_fully_loaded( song_info_t *song_info );
void *playlist_manager( void *input_data );
void save_playlist( void );
void load_playlist( void );
void playlist_queue_song( song_info_t *song );

#endif
