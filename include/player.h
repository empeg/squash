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
 * player.h
 */
#ifndef SQUASH_PLAYER_H
#define SQUASH_PLAYER_H

#define PLAYER_MAX_BUFFER_SIZE 128
#define PLAYER_MAX_BUFFER_PCM_SIZE (128*1024)

/*
 * Global Data
 */
extern song_functions_t song_functions[];

/*
 * Prototypes
 */
void *frame_decoder( void *input_data );
void *player( void *input_data );
song_info_t *get_next_song_info( void );
void done_with_song_info( song_info_t *song );
int detect_silence( frame_data_t frame_data, unsigned int *silence_duration );
void set_now_playing_info( song_info_t *song );
double *get_spectrum(char *pcm_data, int pcm_length);
void player_queue_command( enum player_command_e command );
#endif
