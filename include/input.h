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
 * input.h
 */

#ifndef SQUASH_INPUT_H
#define SQUASH_INPUT_H

/*
 * Structures
 */
typedef struct fifo_info_s {
    FILE *fifo_file;
} fifo_info_t;

fifo_info_t fifo_info;

/*
 * Enumerations
 */
enum direction_e { UP, DOWN, LEFT, RIGHT, PAGE_UP, PAGE_DOWN, HOME, END };

/*
 * Prototypes
 */
void *keyboard_monitor( void *input_data );
void *fifo_monitor( void *input_data );

void do_clear( void );
void do_resize( void );
void do_quit( void );

void do_set_player_command( enum player_command_e );
void do_toggle_player_command( void );

void do_set_spectrum_state( enum window_states_e new_state  );
void do_toggle_spectrum_state( void  );

void do_toggle_info_command( void );

void do_arrow( enum direction_e direction );
void do_delete( void );
void do_tab( void );

void do_edit_command( void );

#endif
