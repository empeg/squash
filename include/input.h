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

#ifdef EMPEG
#define IR_TOP_BUTTON_PRESSED           0x00000000
#define IR_TOP_BUTTON_RELEASED          0x00000001
#define IR_RIGHT_BUTTON_PRESSED         0x00000002
#define IR_RIGHT_BUTTON_RELEASED        0x00000003
#define IR_LEFT_BUTTON_PRESSED          0x00000004
#define IR_LEFT_BUTTON_RELEASED         0x00000005
#define IR_BOTTOM_BUTTON_PRESSED        0x00000006
#define IR_BOTTOM_BUTTON_RELEASED       0x00000007
#define IR_KNOB_PRESSED                 0x00000008
#define IR_KNOB_RELEASED                0x00000009
#define IR_KNOB_RIGHT                   0x0000000a
#define IR_KNOB_LEFT                    0x0000000b
#define IR_0                            0x00B94600
#define IR_1                            0x00B94601
#define IR_2                            0x00B94602
#define IR_3                            0x00B94603
#define IR_4                            0x00B94604
#define IR_5                            0x00B94605
#define IR_6                            0x00B94606
#define IR_7                            0x00B94607
#define IR_8                            0x00B94608
#define IR_9                            0x00B94609
#define IR_TRACK_MINUS                  0x00B9460A
#define IR_TRACK_PLUS                   0x00B9460B
#define IR_AM                           0x00B9460C
#define IR_FM                           0x00B9460D
#define IR_PROG                         0x00B9460E
#define IR_DIRECT                       0x00B9460F
#define IR_VOL_PLUS                     0x00B94614
#define IR_VOL_MINUS                    0x00B94615
#define IR_STAR                         0x00B9461B
#define IR_TUNER                        0x00B9461C
#define IR_TAPE                         0x00B9461D
#define IR_CD                           0x00B9461E
#define IR_CDMDCH                       0x00B9461F
#define IR_DNPP                         0x00B9465E
#endif

/*
 * Prototypes
 */
void process_ir_event( long button, bool held );
void *ir_monitor( void *input_data );
void *keyboard_monitor( void *input_data );
void *fifo_monitor( void *input_data );

#ifndef NO_NCURSES
void do_clear( void );
void do_resize( void );
#endif
void do_quit( void );

void do_set_player_command( enum player_command_e );
void do_toggle_player_command( void );

#ifndef NO_NCURSES
void do_set_spectrum_state( enum window_states_e new_state  );
void do_toggle_spectrum_state( void  );

void do_toggle_info_command( void );

void do_arrow( enum direction_e direction );
void do_delete( void );
void do_tab( void );

void do_edit_command( void );
#endif

#endif
