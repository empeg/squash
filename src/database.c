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
 * database.c
 */
#include "global.h"
#include "stat.h"       /* for start_song_picker() */
#include "player.h"     /* for player_queue_command() */
#include "display.h"    /* for draw_info() and draw_screen() */
#include "database.h"

/* File Extensions to use. */
const db_extension_info_t db_extensions[] = {
    {"info", BASENAME_META, insert_meta_data, NULL, NULL },
    {"stat", BASENAME_STAT, set_stat_data, save_stat_data, is_stat_data_changed}
};
const int db_extensions_size = sizeof( db_extensions ) / sizeof( db_extensions[0] );

/*
 * This is called by main() to load the DB.  Since nothing else can happen until
 * the database is loaded, it does a few other things, in order to tell the other
 * threads that the database has loaded, and that they can start.
 */
void *setup_database( void *data ) {
    /* Grab our locks */
    squash_lock( display_info.lock );
    squash_wlock( database_info.lock );
    squash_lock( song_queue.lock );
    squash_lock( player_info.lock );

    /* Load the database */
    load_db_filenames();

    /* Unlock the locks we don't need anymore */
    squash_unlock( player_info.lock );
    squash_unlock( song_queue.lock );

    /* Hide the info window, and unhide the spectrum analyzer */
    display_info.window[ WIN_INFO ].state = WIN_STATE_HIDDEN;
    display_info.window[ WIN_SPECTRUM ].state = WIN_STATE_NORMAL;
    display_info.state = SYSTEM_RUNNING;

    /* Tell the specturm analyzer it can run */
    squash_lock( spectrum_ring.lock );
    spectrum_ring.active = TRUE;
    squash_unlock( spectrum_ring.lock );

    /* Update the screen */
    squash_wunlock( database_info.lock );
    draw_screen();

    /* Release the database lock */
    squash_unlock( display_info.lock );

    /* Tell the player to start playing */
    squash_lock( player_command.lock );
    player_queue_command( CMD_PLAY );
    squash_signal( player_command.changed );
    squash_unlock( player_command.lock );

    /* Tell the playlist_manager() thread to add songs */
    squash_lock( song_queue.lock );
    song_queue.wanted_size = config.playlist_manager_playlist_size;
    squash_signal( song_queue.not_full );
    squash_unlock( song_queue.lock );

    load_all_meta_data( 1 ); /* Load statistics */
    /* Load the statistics routine (needed for playlist_manager() to call pick_song()) */
    start_song_picker();
    squash_signal( database_info.stats_finished );

    load_all_meta_data( 0 ); /* Load info files */

    return (void *)NULL;
}

/*
 * Loads the database.  Will walk the config.db_paths[ BASENAME_SONG ] directory
 * looking for known music types.  Loading the meta data and stat values are done,
 * at a later time, this only loads filenames.
 */
void load_db_filenames( void ) {
    struct stat path_stat;

    /* Initialize database_info */
    if( database_info.songs != NULL ) {
        squash_free( database_info.songs );
        database_info.song_count = 0;
        database_info.song_count_allocated = 0;
        database_info.songs = NULL;
        database_info.stats_loaded = 0;
    }

    /* Check the path to see if it exists */
    if( stat(config.db_paths[ BASENAME_SONG ], &path_stat) != 0 ) {
        squash_error( "The path or file '%s' does not exist or I can't open it", config.db_paths[ BASENAME_SONG ] );
    }

    /* We need to walk the directory tree and load bunches of files */
    _walk_filesystem( NULL, _load_file );

    squash_log("Songs loaded %d", database_info.song_count);

    if( database_info.song_count == 0 ) {
        squash_error( "There are no songs in the path '%s'", config.db_paths[ BASENAME_SONG ] );
    }

    /* Trim database_info.songs[]'s allocated size */
    database_info.song_count_allocated = database_info.song_count;
    squash_realloc( database_info.songs, database_info.song_count_allocated * sizeof(song_info_t) );
}

/*
 * Returns the values for a particular key within a song entry
 * (e.g. Heart, for key artist).
 */
meta_key_t *get_meta_data( song_info_t *song_info, char *meta_key ) {
    int i;

    /* Make sure we were given complete information */
    if( song_info == NULL ) {
        return (meta_key_t *)NULL;
    }

    /* Find the first matching key */
    for( i = 0; i < song_info->meta_key_count; i++ ) {
        if( strcasecmp(meta_key, song_info->meta_keys[i].key) == 0 ) {
            return &song_info->meta_keys[i];
        }
    }

    /* We did not find it */
    return (meta_key_t *)NULL;
}

/*
 * Finds the first song that has a matching filename
 * Expects filename to be a full path, and matches against
 * the BASE_NAME_SONG full name of each song.
 * TODO: This could be faster if we sort the database by filename
 */
song_info_t *find_song_by_filename( char *filename ) {
    int i;
    song_info_t *song;
    int basename_length;

    /* Someone is fooling us. */
    if( filename == NULL ) {
        return NULL;
    }

    /* Search database for the first matching filename */
    for( i = 0; i < database_info.song_count; i++ ) {
        song = &database_info.songs[i];

        /* figure out where would the basename ends (if the basenames match) */
        basename_length = strlen( song->basename[ BASENAME_SONG ] );
        /* Check if the filenames match first (since this is more likely to be different */
        if( strcmp( song->filename, &filename[basename_length+1] ) == 0 ) {
            /* Check if there is a '/' and that the bases match */
            if( '/' == filename[basename_length] &&
                strncmp( song->basename[ BASENAME_SONG ], filename, basename_length ) == 0) {
                return song;
            }
        }
    }

    /* No match */
    return NULL;
}

/*
 * Find any songs whose values match keyword.  Looks only at a particular
 * key.
 */
db_search_result_t find_matches( char *key, char *keyword ) {
    int found_counter;
    int i, j, k;
    char *match;
    db_search_result_t search_result;
    song_info_t *song;
    meta_key_t *meta;

    found_counter = 0;
    search_result.song_count = 0;
    search_result.songs = NULL;

    /* for each entry in the database */
    for( i = 0; i < database_info.song_count; i++ ) {
        song = &database_info.songs[i];
        /* for each meta key in a song */
        for( j = 0; j < song->meta_key_count; j++ ) {
            meta = &database_info.songs[i].meta_keys[j];
            /* if the key matches */
            if( strcmp( key, meta->key ) == 0 ) {
                /* for each value */
                for( k = 0; k < meta->value_count; k++ ) {
                    match = strstr( keyword, meta->values[k] );
                    /* If a match was found add it to our result set */
                    if( match != NULL ) {
                        squash_ensure_alloc( found_counter, search_result.song_count, search_result.songs, sizeof(song_info_t *), 10, *=2 );
                        search_result.songs[ found_counter++ ] = song;
                    }
                }

                /* there shouldn't be any other keys that match */
                break;
            }
        }
    }

    /* Trim the allocated size of the result set */
    search_result.song_count = found_counter;
    squash_realloc( search_result.songs, search_result.song_count * sizeof(song_info_t *) );

    return search_result;
}

/*
 * Clears everything allocate in a song except for the filename
 */
void clear_song_meta( song_info_t *song ) {
    int j, k;

    /* For each meta key and the meta key's values, free the memory */
    for( j = 0; j < song->meta_key_count; j++ ) {
        /* Free each of the meta key's values */
        for( k = 0; k < song->meta_keys[j].value_count; k++ ) {
            squash_free( song->meta_keys[j].values[k] );
        }

        /* Free the array used to hold the values */
        squash_free( song->meta_keys[j].values );

        /* Free the meta key */
        squash_free( song->meta_keys[j].key );
    }
    song->meta_keys = NULL;
    song->meta_key_count = 0;
    song->stat.changed = FALSE;

    squash_free( song->meta_keys );
}

/*
 * Free the allocate memory by database and indices
 */
void clear_db() {
    int i;

    /* For each song free the meta keys and values and the filename */
    for( i = 0; i < database_info.song_count; i++ ) {
        /* Free the meta keys and values */
        clear_song_meta( &database_info.songs[i] );

        /* Free the filename */
        squash_free( database_info.songs[i].filename );
    }

    /* Free the array of songs */
    squash_free( database_info.songs );

    /* Reset the sizes */
    database_info.song_count = 0;
    database_info.song_count_allocated = 0;
}

/*
 * Save a song entry back to disk.
 * NOTE: this is a nice abstracted routine, but it currently only
 * really does any work for the ".stat" files; not the ".info" files
 */
void save_song( song_info_t *song ) {
    int cur_file_length;
    char *cur_filename;
    FILE *cur_file;
    int i;

    /* Quit if song is invalid or we are supposed to be read only */
    if( song == NULL || config.db_readonly ) {
        return;
    }

    /* For each extension save any changes */
    for( i = 0; i < db_extensions_size; i++ ) {
        /* Quit if there is no is_changed() procedure, or is_changed() says
           there are no changes to save */
        if( db_extensions[i].is_changed == NULL || !db_extensions[i].is_changed( song ) ) {
            continue;
        }

        /* Construct a filename to save */
        cur_file_length = strlen(song->basename[db_extensions[i].which_basename]) + strlen(song->filename) + strlen(db_extensions[i].extension) + 3;
        squash_malloc( cur_filename, cur_file_length );

        snprintf( cur_filename, cur_file_length, "%s/%s.%s", song->basename[db_extensions[i].which_basename], song->filename, db_extensions[i].extension);

        /* If you can't open the file bomb */
        if( (cur_file = fopen(cur_filename, "w")) == NULL ) {
            squash_error( "Can't open file \"%s\" for writing", cur_filename );
        }

        /* Save the data */
        db_extensions[i].save_data( song, cur_file );

        /* Clean up */
        fclose(cur_file);
        squash_free(cur_filename);
    }
}

/* Used by save song to tell if the ".stat" file needs to be saved */
bool is_stat_data_changed( song_info_t *song ) {
    /* Return false if song is null */
    if( song == NULL ) {
        return FALSE;
    }

    /* Return the value */
    return song->stat.changed;
}

/*
 * Save statistical info.  This is should be called by save_song()
 */
void save_stat_data( song_info_t *song, FILE *file ) {
    /* Some one is tricking us */
    if( song == NULL || file == NULL ) {
        return;
    }

    /* Save the values */
    fprintf( file, "play_count=%d\n", song->stat.play_count );
    fprintf( file, "skip_count=%d\n", song->stat.skip_count );
    fprintf( file, "repeat_counter=%d\n", song->stat.repeat_counter );

    /* Reset the changed flag */
    song->stat.changed = FALSE;
}

/*
 * Loads the metadata for a song from the disk
 */
void load_meta_data( song_info_t *song, int which ) {
    char *cur_file;
    int cur_file_length;

    /* Don't let them trick us! */
    if( song == NULL || song->filename == NULL ) {
        return;
    }

    /* Determine the length of the filename to open */
    cur_file_length = strlen(song->basename[db_extensions[which].which_basename]) + strlen(song->filename) + strlen(db_extensions[which].extension) + 3;

    /* Allocate memory to build the filename */
    squash_malloc( cur_file, cur_file_length );

    /* Build the filename */
    snprintf( cur_file, cur_file_length, "%s/%s.%s", song->basename[db_extensions[which].which_basename], song->filename, db_extensions[which].extension);

    /* Parse the file */
    parse_file( cur_file, db_extensions[which].add_data, (void *)song );

    /* Free the file name */
    squash_free( cur_file );
}

/*
 * Goes though all songs and loads the meta data for them.
 */
void load_all_meta_data( int which ) {
    int i;
    for( i = 0; i < database_info.song_count; i++ ) {
        squash_wlock( database_info.lock );
        if( which == 0 ) {
            if( database_info.songs[i].meta_key_count == -1 ) {
                database_info.songs[i].meta_key_count = 0;
            } else {
                squash_wunlock( database_info.lock );
                continue;
            }
        }
        load_meta_data( &database_info.songs[i], which );
        squash_wunlock( database_info.lock );
    }
}


/*
 * Insert a key and value into a song entry.
 * Header should be copied, key and value are already copied.
 */
void insert_meta_data( void *data, char *header, char *key, char *value ) {
    song_info_t *song = (song_info_t *)data;
    int i;
    meta_key_t *meta_key;

    /* Make sure we were given at least a song and a key */
    if( (song == NULL) || (key == NULL) ) {
        return;
    }

    /* Locate the key data */
    meta_key = NULL;
    for( i = 0; i < song->meta_key_count; i++ ) {
        /* On a match set meta_key, and break */
        if( strcasecmp(key, song->meta_keys[i].key) == 0 ) {
            meta_key = &song->meta_keys[i];
            break;
        }
    }

    /* Allocate space for a new key structure if one wasn't found */
    if( meta_key == NULL ) {
        song->meta_key_count++;
        squash_realloc( song->meta_keys, song->meta_key_count * sizeof(meta_key_t) );

        meta_key = &song->meta_keys[ song->meta_key_count - 1 ];
        meta_key->key = key;
        meta_key->value_count = 0;
        meta_key->values = NULL;
    }

    /* Increase size of space for the meta info */
    meta_key->value_count++;
    squash_realloc( meta_key->values, meta_key->value_count * sizeof(char **) );

    /* Add value */
    meta_key->values[ meta_key->value_count - 1 ] = value;
}

/*
 * Set the statistical data for a song entry.
 * Header should be copied, key and value are already copied.
 */
void set_stat_data( void *data, char *header, char *key, char *value ) {
    song_info_t *song = (song_info_t *)data;
    int int_value;

    /* Make sure we were given at least a song and a key */
    if( (song == NULL) || (key == NULL) ) {
        return;
    }

    /* Convert to an integer */
    int_value = atoi( value );

    /* Add it to the structure */
    if( strncasecmp("play_count", key, 11) == 0 ) {
        song->stat.play_count = int_value;
    } else if( strncasecmp("repeat_counter", key, 15) == 0 ) {
        song->stat.repeat_counter = int_value;
    } else if( strncasecmp("skip_count", key, 11) == 0 ) {
        song->stat.skip_count = int_value;
    }
}

/*
 * Load a song into the database.  This is called from _walk_filesystem()
 */
void _load_file( char *filename ) {
    enum song_type_e type;
    song_info_t *song;

    /* Avoid mistakes */
    if( filename == NULL ) {
        return;
    }

    /* Determine the song type */
    type = get_song_type( filename );

    /* If filename is a known music type, load it into the database */
    if( type != TYPE_UNKNOWN ) {
        /* Make sure we have enough memory */
        squash_ensure_alloc( database_info.song_count, database_info.song_count_allocated,
                database_info.songs, sizeof(song_info_t), INITIAL_DB_SIZE, *=2 );

        /* Setup the database entry */
        song = &database_info.songs[ database_info.song_count ];
        song->filename = strdup(filename);
        song->basename[ BASENAME_SONG ] = config.db_paths[ BASENAME_SONG ];
        song->basename[ BASENAME_META ] = config.db_paths[ BASENAME_META ];
        song->basename[ BASENAME_STAT ] = config.db_paths[ BASENAME_STAT ];
        song->meta_keys = NULL;
        song->meta_key_count = -1;
        song->stat.changed = FALSE;

        /* Update the counter */
        database_info.song_count++;

        /* Display progress every 100 songs */
        if( database_info.song_count % 100 == 0) {
            draw_info();
        }
    }
}

/*
 * Walk a filesystem and call loader() on any files found
 */
void _walk_filesystem( char *base_dir, void(*loader)(char *) ) {
    DIR *dir;
    struct dirent *file_entry;
    struct stat file_stat;
    char *file_path = NULL;
    char *fullfile_path = NULL;
    char *fullbase_dir = NULL;
    int file_entry_length;
    int base_dir_length, fullbase_dir_length;

    /* Set the fullbase_dir */
    if( base_dir == NULL ) {
        fullbase_dir = strdup( config.db_paths[ BASENAME_SONG ] );
        base_dir_length = 0;
    } else {
        squash_asprintf(fullbase_dir, "%s/%s", config.db_paths[ BASENAME_SONG ], base_dir );
        base_dir_length = strlen( base_dir );
    }
    fullbase_dir_length = strlen( fullbase_dir );

    /* Open the directory */
    if( (dir = opendir(fullbase_dir)) == NULL ) {
        /* Directory won't open (probably permissions) */
        return;
    }

    /* For each directory entry, recursively call ourselves if it's a directory
       otherwise call loader() if it's a file */
    if( base_dir != NULL ) {
        squash_malloc( file_path, base_dir_length + 257 );
        strcpy( file_path, base_dir );
        file_path[ base_dir_length ] = '/';
        file_path[ base_dir_length+1 ] = '\0';
    }
    squash_malloc( fullfile_path, fullbase_dir_length + 257 );
    strcpy( fullfile_path, fullbase_dir );
    fullfile_path[ fullbase_dir_length ] = '/';
    fullfile_path[ fullbase_dir_length+1 ] = '\0';
    while( (file_entry = readdir(dir)) != NULL ) {
        /* Ignore current/parent directory entries */
        if( (strncmp(".", file_entry->d_name, 2) == 0) ||
            (strncmp("..", file_entry->d_name, 3) == 0)) {
            continue;
        }
        file_entry_length = strlen( file_entry->d_name );

        /* Build file_path */
        if( base_dir == NULL ) {
            file_path = file_entry->d_name;
        } else {
            strcpy( &file_path[ base_dir_length + 1 ], file_entry->d_name );
        }
        /* Bulid fullfile_path */
        strcpy( &fullfile_path[ fullbase_dir_length + 1 ], file_entry->d_name );

        /* If the file can be stat'd, continue walking process */
        if( stat(fullfile_path, &file_stat) == 0 ) {
            /* If it's a directory call ourselves again,
               otherwise load this file with loader() */
            if( S_ISDIR(file_stat.st_mode) ) {
                _walk_filesystem( file_path, loader );
            } else {
                loader( file_path );
            }
        }

    }
    /* Free built names */
    if( base_dir != NULL ) {
        squash_free( file_path );
    }
    squash_free( fullfile_path );

    /* Free full_basedir */
    squash_free( fullbase_dir );

    /* Close directory */
    closedir( dir );
}
