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
 * display.h
 */
#ifndef SQUASH_DISPLAY_H
#define SQUASH_DISPLAY_H

/*
 * Prototypes
 */
void display_init( void );
void window_init( void );

/* Display Thread */
void *display_monitor( void *input_data );

/* Draw Sections of the Screen */
void draw_screen( void );
void draw_now_playing( void );
void draw_playlist( void );
void draw_help( void );
void draw_info( void );
void draw_spectrum( void );

/* Helper Functions */
void draw_meta_string( WINDOW *win, song_info_t *song, char *meta_key, int top, int left, int width );
int num_chars( long value );

#endif
