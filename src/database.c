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
#include "play_ogg.h"   /* for ogg_load_meta() */
#include "play_mp3.h"   /* for mp3_load_meta() */
#include "play_flac.h"  /* for flac_load_meta() */
#ifdef EMPEG
#include "vfdlib.h"     /* for vfdlib_*() */
#include "version.h"    /* for SQUASH_VERSION */
#endif
#include "database.h"

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
#ifndef NO_NCURSES
    display_info.window[ WIN_INFO ].state = WIN_STATE_HIDDEN;
    display_info.window[ WIN_SPECTRUM ].state = WIN_STATE_NORMAL;
#endif

    /* Tell the rest of the system we are done loading the file list */
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

    load_all_meta_data( TYPE_STAT ); /* Load statistics */
    /* Load the statistics routine (needed for playlist_manager() to call pick_song()) */
    start_song_picker();
    squash_signal( database_info.stats_finished );
    squash_log("stats loaded");

    /* This isn't necessary anymore, since we load metadata on playlist load instead.
     * (Will be needed again once searching has been added). */
#if 0
    load_all_meta_data( TYPE_META ); /* Load info files */
    squash_log("metadata loaded");
#endif

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

    /* If we are supposed to use a master list and it already exists */
    if( config.db_masterlist_path != NULL && stat(config.db_masterlist_path, &path_stat) == 0 ) {
        load_masterlist();
    } else {
        /* Check the path to see if it exists */
        if( stat(config.db_paths[ BASENAME_SONG ], &path_stat) != 0 ) {
            squash_error( "The path or file '%s' does not exist or I can't open it", config.db_paths[ BASENAME_SONG ] );
        }

        /* We need to walk the directory tree and load bunches of files */
        _walk_filesystem( NULL, _load_file );
    }

    squash_log("Songs loaded %d", database_info.song_count);

    if( database_info.song_count == 0 ) {
        squash_error( "There are no songs in the path '%s'", config.db_paths[ BASENAME_SONG ] );
    }

    /* Trim database_info.songs[]'s allocated size */
    database_info.song_count_allocated = database_info.song_count;
    squash_realloc( database_info.songs, database_info.song_count_allocated * sizeof(song_info_t) );

    if( config.db_masterlist_path != NULL ) {
        save_masterlist();
    }
}

void load_masterlist( void ) {
    FILE *masterlist_file;
    struct stat file_info;
    char *file_data;
    char *cur_data;
    char *this_line;

    /* Open the file to write out the playlist */
    if( (masterlist_file = fopen(config.db_masterlist_path, "r")) == NULL ) {
        squash_log("Couldn't open masterlist file, probably didn't exist");
        return;
    }

    if( fstat(fileno(masterlist_file), &file_info) ) {
        squash_log("Couldn't stat masterlist file");
        fclose( masterlist_file );
        return;
    }

    if( (file_data = (char *)mmap( NULL, file_info.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fileno(masterlist_file), 0)) == MAP_FAILED ) {
        squash_log("Couldn't mmap masterlist file");
        fclose( masterlist_file );
        return;
    }

    cur_data = file_data;
    while( cur_data != NULL ) {
        this_line = strsep( &cur_data, "\n" );
        /* if not a line, empty, or is a comment */
        if( !(this_line == NULL || strlen(this_line) == 0 || this_line[0] == '#') ) {
            _load_file( this_line, TRUE );
        }
    }

    /* Close the playlist file */
    fclose( masterlist_file );
}

void save_masterlist( void ) {
    FILE *masterlist_file;
    time_t current_time;
    int i;

    if( config.db_readonly ) {
        return;
    }

    /* Open the file to write out the masterlist */
    if( (masterlist_file = fopen(config.db_masterlist_path, "w")) == NULL ) {
        squash_error( "Can't open file \"%s\" for writing", config.db_masterlist_path );
    }

    /* Write out header */
    time( &current_time );
    fprintf( masterlist_file, "# Saved Squash masterlist\n" );
    fprintf( masterlist_file, "# Auto-Generated on %s\n", ctime(&current_time) );

    /* Write out the masterlist */
    for( i = 0; i < database_info.song_count; i++ ) {
        fprintf( masterlist_file, "%s\n", database_info.songs[i].filename );
    }

    /* Close the masterlist file */
    fclose( masterlist_file );

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
 * Clears everything allocated in a song except for the filename
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
    char *cur_filename;
    char *end, *dirname;
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
        cur_filename = build_fullfilename( song, db_extensions[i].which_basename );

        /* find dir part */
        end = strrchr( cur_filename, '/' );
        if( end != NULL ) {
            dirname = copy_string( cur_filename, end - 1 );
            /* ensure path is created */
            create_path( dirname );
            squash_free( dirname );
        }

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
    fprintf( file, "\n" );

    /* Reset the changed flag */
    song->stat.changed = FALSE;
}

/*
 * Save statistical info.  This is should be called by save_song()
 */
void save_meta_data( song_info_t *song, FILE *file ) {
    int i, j;

    /* Some one is tricking us */
    if( song == NULL || file == NULL ) {
        return;
    }

    for(i = 0; i < song->meta_key_count; i++ ) {
        for( j = 0; j < song->meta_keys[i].value_count; j++ ) {
            fprintf( file, "%s=%s\n", song->meta_keys[i].key, song->meta_keys[i].values[j] );
        }
    }
    fprintf( file, "\n" );
}

/*
 * Loads the metadata for a song from the disk
 */
void load_meta_data( song_info_t *song, enum meta_type_e which ) {
    char *filename, *metaname, *dirname;
    char *end;
    bool success;
    FILE *meta_file;
    struct stat file_stat;

    /* Don't let them trick us! */
    if( song == NULL ) {
        return;
    }

    /* If we haven't been loaded before and this is TYPE_META, set meta_key_count to 0 */
    if( which == TYPE_META && song->meta_key_count == -1 ) {
        song->meta_key_count = 0;
    }

    metaname = NULL;
    /* If this is an empeg, we may be reading the original empeg's player's
     * database files.  In which case music files we find that end it a 0,
     * will probably have a meta file that ends instead with 1.  We will
     * load that instead of the normal .info file. */
#ifdef EMPEG
    if( which == TYPE_META ) {
        int length;
        length = strlen(song->filename);
        if( length > 0 && song->filename[length-1] == '0' ) {
            song->filename[length-1] = '1';
            metaname = build_fullfilename( song, BASENAME_SONG );
            song->filename[length-1] = '0';
        }
    }
#endif
    if( metaname == NULL ) {
        metaname = build_fullfilename( song, db_extensions[which].which_basename );
    }

    /* Parse the file unless we want to reload/save from the original song file */
    if( config.db_overwriteinfo && config.db_saveinfo && which == TYPE_META ) {
        success = FALSE;
    } else {
        success = parse_file( metaname, db_extensions[which].add_data, (void *)song );
    }

    /* If we didn't load from the stat/info file, and we have a info file,
     * try to load it from the original song file */
    if( !success && which == TYPE_META ) {
        /* perhaps we haven't figured out what type of song it is yet */
        if( song->song_type == -1 ) {
            song->song_type = get_song_type( song->basename[BASENAME_SONG], song->filename );
        }

        if( song->song_type != TYPE_UNKNOWN ) {
            filename = build_fullfilename( song, BASENAME_SONG );

            switch( song->song_type ) {
                case TYPE_OGG:
                    ogg_load_meta( (void *)song, filename );
                    break;
                case TYPE_MP3:
                    mp3_load_meta( (void *)song, filename );
                    break;
                case TYPE_FLAC:
                    flac_load_meta( (void *)song, filename );
                    break;
                case TYPE_UNKNOWN:
                    /* can't get here */
                    break;
            }
            squash_free( filename );

            if( config.db_saveinfo ) {
#ifdef NO_VORBIS_COMMENT
                if( song->song_type != TYPE_OGG ) {
#else
                if( TRUE ) {
#endif
#ifdef NO_ID3LIB
                if( song->song_type != TYPE_MP3 ) {
#else
                if( TRUE ) {
#endif
                /* find dir part */
                end = strrchr( metaname, '/' );
                if( end != NULL ) {
                    dirname = copy_string( metaname, end - 1 );
                    /* ensure path is created */
                    create_path( dirname );
                    squash_free( dirname );
                }

                /* check to see if the file already exists and don't save unless
                 * we are supposed to overwrite */
                if( stat( metaname, &file_stat ) != 0 || config.db_overwriteinfo ) {
                    /* If you can't open the file bomb */
                    if( (meta_file = fopen(metaname, "w")) == NULL ) {
                        squash_error( "Can't open file \"%s\" for writing", metaname );
                    }
                    save_meta_data( song, meta_file );
                    fclose(meta_file);
                }
/* close two #ifdefs: */ }}
            }
        }
    }

    /* Free the file name */
    squash_free( metaname );
}

/*
 * Goes though all songs and loads the meta data for them.
 */
void load_all_meta_data( enum meta_type_e which ) {
    int i;
    for( i = 0; i < database_info.song_count; i++ ) {
        squash_wlock( database_info.lock );
        /* If the type is meta, we need to be careful, and not load this song a second time  */
        if( which == TYPE_META && database_info.songs[i].meta_key_count != -1 ) {
            squash_wunlock( database_info.lock );
            continue;
        }
        load_meta_data( &database_info.songs[i], which );
        squash_wunlock( database_info.lock );
        sched_yield();
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
        /* shouldn't happen, but let's be safe */
        if( value != NULL ) {
            squash_free( value );
        }
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
    } else {
        /* Otherwise we already have a copy of key, so free this extra copy */
        squash_free( key );
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
        /* shouldn't happen, but let's be safe */
        if( value != NULL ) {
            squash_free( value );
        }
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

    squash_free( key );
    squash_free( value );
}

/*
 * Load a song into the database.  This is called from _walk_filesystem()
 */
void _load_file( char *filename, bool trust ) {
    enum song_type_e type = TYPE_UNKNOWN;
    song_info_t *song;

    /* Avoid mistakes */
    if( filename == NULL ) {
        return;
    }

    /* Determine the song type unless we trust this file */
    if( !trust ) {
        type = get_song_type( config.db_paths[ BASENAME_SONG ], filename );
    }

    /* If filename is a known music type or we trust it, load it into the database */
    if( type != TYPE_UNKNOWN || trust ) {
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
        song->stat.play_count = 0;
        song->stat.skip_count = 0;
        song->stat.repeat_counter = 0;
        song->stat.changed = FALSE;
        song->play_length = -1;
        song->song_type = -1;

        /* Update the counter */
        database_info.song_count++;

#ifdef EMPEG
        if( database_info.song_count % 50 == 0 ) {
            int x = database_info.song_count, i = 3;
            char buf[5] = "0000";
            while( i >= 0 && x > 0 ) {
                buf[i] += x % 10;
                x /= 10;
                i--;
            }
            vfdlib_clear( display_info.screen, 0 );
            vfdlib_drawText( display_info.screen, "Squash Loading, version " SQUASH_VERSION, 0, 0, 2, 3);
            vfdlib_drawText( display_info.screen, buf, 0, 6, 2, 3 );
            ioctl(display_info.screen_fd, _IO('d', 0));
        }
#endif
#ifndef NO_NCURSES
        /* Display progress every 500 songs */
        if( database_info.song_count % 500 == 0) {
            draw_info();
        }
#endif
    }
}

/*
 * Walk a filesystem and call loader() on any files found
 */
void _walk_filesystem( char *base_dir, void(*loader)(char *, bool) ) {
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
                loader( file_path, FALSE );
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
