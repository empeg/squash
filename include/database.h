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
 * database.h
 */
#ifndef SQUASH_DATABASE_H
#define SQUASH_DATABASE_H

#include <dirent.h> /* For readdir(), etc. */
#include <pthread.h>

/* Initial database allocation size */
#define INITIAL_DB_SIZE 2500

/*
 * Prototypes
 */
void *setup_database( void *data );

void load_db_filenames( void );
void load_masterlist( void );
void save_masterlist( void );
void clear_song_meta( song_info_t *song );
void clear_db( void );

meta_key_t *get_meta_data( song_info_t *song_info, char *meta_key );
song_info_t *find_song_by_filename( char *filename );
db_search_result_t find_matches( char *key, char *keyword );

void load_meta_data( song_info_t *song, enum meta_type_e which );
void load_all_meta_data( enum meta_type_e which );
void save_song( song_info_t *song );

void insert_meta_data( void *data, char *header, char *key, char *value );
void save_meta_data( song_info_t *song, FILE *file );
bool is_meta_data_changed( song_info_t *song );

void set_stat_data( void *data, char *header, char *key, char *value );
void save_stat_data( song_info_t *song, FILE *file );
bool is_stat_data_changed( song_info_t *song );

void _load_file( char *filename, bool trust );
void _walk_filesystem( char *base_dir, void(*loader)(char *, bool) );

#endif
