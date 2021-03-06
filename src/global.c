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
 * global.c
 * Global Functions
 */

#include "global.h"
#include "database.h"
#include "display.h"
#include "sound.h" /* for sound_set_volume() */

/* File Extensions to use. */
const db_extension_info_t db_extensions[] = {
    {"info", BASENAME_META, insert_meta_data, save_meta_data, NULL },
    {"stat", BASENAME_STAT, set_stat_data, save_stat_data, is_stat_data_changed}
};
const int db_extensions_size = sizeof( db_extensions ) / sizeof( db_extensions[0] );

/*
 * Setup the config file keys
 */
config_keys_t config_keys[ CONFIG_KEY_COUNT ] = {
    { "Database", "Info_Path", (void *)&config.db_paths[ BASENAME_META ], TYPE_STRING },
    { "Database", "Stat_Path", (void *)&config.db_paths[ BASENAME_STAT ], TYPE_STRING },
    { "Database", "Song_Path", (void *)&config.db_paths[ BASENAME_SONG ], TYPE_STRING },
    { "Database", "Masterlist_Filename", (void *)&config.db_masterlist_path, TYPE_STRING },
    { "Database", "Readonly", (void *)&config.db_readonly, TYPE_INT },
    { "Database", "Save_Info", (void *)&config.db_saveinfo, TYPE_INT },
    { "Database", "Overwrite_Info", (void *)&config.db_overwriteinfo, TYPE_INT },
    { "Database", "Manual_Rating_Bias", (void *)&config.db_manual_rating_bias, TYPE_DOUBLE },
    { "Global", "State_Filename", (void *)&config.global_state_path, TYPE_STRING },
    { "Global", "Control_Filename", (void *)&config.input_fifo_path, TYPE_STRING },
#ifdef DEBUG
    { "Global", "Log_Filename", (void *)&config.squash_log_path, TYPE_STRING },
#endif
#ifdef EMPEG_DSP
    { "Empeg", "save_volume_minimum", (void *)&config.min_save_volume, TYPE_INT },
    { "Empeg", "save_volume_maximum", (void *)&config.max_save_volume, TYPE_INT },
#endif
    { "Playlist", "Size", (void *)&config.playlist_manager_playlist_size, TYPE_INT },
    { "Pastlist", "Size", (void *)&config.playlist_manager_pastlist_size, TYPE_INT }
};

#ifdef EMPEG
/*
 * Setup empeg menus
 */
menu_list_t menu_list[ EMPEG_SCREEN_COUNT ] = (menu_list_t []) {
    { (menu_item_t []){
            (menu_item_t) { "Main Menu",            0, EMPEG_SCREEN_MAIN_MENU, FALSE },
            (menu_item_t) { "Currently Playing",    1, EMPEG_SCREEN_PLAY, TRUE },
            (menu_item_t) { "Upcoming Songs",       1, EMPEG_SCREEN_PLAYLIST, TRUE },
            (menu_item_t) { "Statistics",           1, EMPEG_SCREEN_GLOBAL_SONG_INFO, TRUE },
            (menu_item_t) { "Quit Squash",          1, EMPEG_SCREEN_QUIT, TRUE },
            (menu_item_t) { "Change Settings",      1, EMPEG_SCREEN_SETTINGS, FALSE }
        },
        6, 0
    },
    { (menu_item_t []){
            (menu_item_t) { "Go Back",              0, EMPEG_SCREEN_MAIN_MENU, FALSE },
            (menu_item_t) { "Currently Playing",    0, EMPEG_SCREEN_PLAY, TRUE },
            (menu_item_t) { "Rate This Song",       1, EMPEG_SCREEN_PLAY_RATE_SONG, TRUE },
            (menu_item_t) { "Song Info",            1, EMPEG_SCREEN_PLAY_SONG_INFO, TRUE }
        },
        4, 1
    },
    { (menu_item_t []){
            (menu_item_t) { "Go Back",              0, EMPEG_SCREEN_PLAY, FALSE },
            (menu_item_t) { "Song Details",         0, EMPEG_SCREEN_PLAY_SONG_INFO, TRUE }
        },
        2, 0
    },
    { (menu_item_t []){
            (menu_item_t) { "Go Back",              0, EMPEG_SCREEN_PLAY, FALSE },
            (menu_item_t) { "Rate This Song",       0, EMPEG_SCREEN_PLAY_RATE_SONG, TRUE }
        },
        2, 0
    },
    { (menu_item_t []){
            (menu_item_t) { "Go Back",              0, EMPEG_SCREEN_MAIN_MENU, FALSE },
            (menu_item_t) { "Upcoming Songs",       0, EMPEG_SCREEN_PLAYLIST, TRUE },
            (menu_item_t) { "Rate This Song",       1, EMPEG_SCREEN_PLAYLIST_RATE_SONG, TRUE },
            (menu_item_t) { "Song Info",            1, EMPEG_SCREEN_PLAYLIST_SONG_INFO, TRUE },
            (menu_item_t) { "Remove from list",     1, EMPEG_SCREEN_PLAYLIST, TRUE }
        },
        5, 1
    },
    { (menu_item_t []){
            (menu_item_t) { "Go Back",              0, EMPEG_SCREEN_PLAYLIST, FALSE },
            (menu_item_t) { "Song Info",            0, EMPEG_SCREEN_PLAYLIST_SONG_INFO, TRUE }
        },
        2, 0
    },
    { (menu_item_t []){
            (menu_item_t) { "Go Back",              0, EMPEG_SCREEN_PLAYLIST, FALSE },
            (menu_item_t) { "Rate This Song",       0, EMPEG_SCREEN_PLAYLIST_RATE_SONG, TRUE }
        },
        2, 0
    },
    { (menu_item_t []){
            (menu_item_t) { "Go Back",              0, EMPEG_SCREEN_MAIN_MENU, FALSE },
            (menu_item_t) { "Statistics",           0, EMPEG_SCREEN_GLOBAL_SONG_INFO, TRUE }
        },
        2, 0
    },
    { (menu_item_t []){
            (menu_item_t) { "Go Back",              0, EMPEG_SCREEN_MAIN_MENU, FALSE },
            (menu_item_t) { "Quit Squash",          0, EMPEG_SCREEN_QUIT, TRUE }
        },
        2, 0
    },
    { (menu_item_t []){
            (menu_item_t) { "Go Back",              0, EMPEG_SCREEN_MAIN_MENU, FALSE },
            (menu_item_t) { "Change Settings",      0, EMPEG_SCREEN_SETTINGS, FALSE },
            (menu_item_t) { "Set Rating Bias",      1, EMPEG_SCREEN_SET_RATING_BIAS, TRUE },
            (menu_item_t) { "Display Brightness",   1, EMPEG_SCREEN_SET_DISPLAY_BRIGHTNESS, TRUE }
        },
        4, 1
    },
    { (menu_item_t []){
            (menu_item_t) { "Go Back",              0, EMPEG_SCREEN_MAIN_MENU, FALSE },
            (menu_item_t) { "Display Brightness",   0, EMPEG_SCREEN_SET_DISPLAY_BRIGHTNESS, TRUE }
        },
        2, 0
    },
    { (menu_item_t []){
            (menu_item_t) { "Go Back",              0, EMPEG_SCREEN_MAIN_MENU, FALSE },
            (menu_item_t) { "Set Rating Bias",      0, EMPEG_SCREEN_SET_RATING_BIAS, TRUE }
        },
        2, 0
    }
};
#endif

void save_state() {
    song_info_t *cur_song;
    long cur_pos;
    FILE *state_file;
    time_t current_time;
    song_queue_entry_t *cur_song_queue_entry;

    if( config.db_readonly ) {
        return;
    }

    /* Open the file to write out the state */
    if( (state_file = fopen(config.global_state_path, "w")) == NULL ) {
        squash_error( "Can't open file \"%s\" for writing", config.global_state_path );
    }

    /* Write out header */
    time( &current_time );
    fprintf( state_file, "# Saved Squash state\n" );
    fprintf( state_file, "# Auto-Generated on %s\n", ctime(&current_time) );

    /* Write out the state */
    cur_song = player_info.song;
    cur_pos = player_info.current_position;
    fprintf(state_file, "current_entry=0\n");
    if( cur_song ) {
        fprintf(state_file, "entry_filename=%s/%s\n", cur_song->basename[ BASENAME_SONG ], cur_song->filename);
        fprintf(state_file, "entry_position=%ld\n", cur_pos);
    }

    for( cur_song_queue_entry = song_queue.head; cur_song_queue_entry != NULL; cur_song_queue_entry = cur_song_queue_entry->next ) {
        cur_song = cur_song_queue_entry->song_info;
        cur_pos = cur_song_queue_entry->start_position;
        fprintf(state_file, "entry_filename=%s/%s\n", cur_song->basename[ BASENAME_SONG ], cur_song->filename);
        fprintf(state_file, "entry_position=%ld\n", cur_pos);
    }

    while( state_info.current_song < state_info.raw_song_count ) {
        fprintf(state_file, "entry_filename=%s\n", state_info.raw_songs[state_info.current_song].filename );
        fprintf(state_file, "entry_position=%ld\n", state_info.raw_songs[state_info.current_song].position );
        state_info.current_song++;
    }

#ifdef EMPEG_DSP
    if( player_info.device ) {
        int volume = player_info.device->volume[0];
        if( volume > config.max_save_volume ) {
            volume = config.max_save_volume;
        } else if( volume < config.min_save_volume ) {
            volume = config.min_save_volume;
        }
        fprintf(state_file, "volume=%d\n", volume);
    }
#endif

#ifdef EMPEG
    fprintf( state_file, "brightness=%d\n", display_info.brightness );
#endif

#ifndef NO_NCURSES
    {
        int i;
        for( i = 0; i < WIN_COUNT; i++ ) {
            fprintf(state_file, "window_number=%d\n", i);
            fprintf(state_file, "window_state=%d\n", display_info.window[i].state);
        }
    }
#endif

    /* Close the state file */
    fclose( state_file );
}

/*
 * Return the file format of the song (mp3, ogg, etc.).
 * TODO: Check using something smarter than just the filename
 */
enum song_type_e get_song_type( const char *base_name, const char *file_name ) {
    enum song_type_e type;
    unsigned int file_name_length;

    /* Initialize data */
    type = TYPE_UNKNOWN;

    /* Determine the length of the file name */
    if( (file_name == NULL) || ((file_name_length = strlen(file_name)) == 0) ) {
        return type;
    }

    /* Determine the correct play functions */
    if( !strncasecmp(file_name + file_name_length - 4, ".mp3", 4) ) {
        type = TYPE_MP3;
    } else if( !strncasecmp(file_name + file_name_length - 4, ".ogg", 4) ) {
        type = TYPE_OGG;
    } else if( !strncasecmp(file_name + file_name_length - 5, ".flac", 5) ) {
        type = TYPE_FLAC;
    }

#ifdef USE_MAGIC
    if( type == TYPE_UNKNOWN ) {
        char buf[4] = {'\0','\0','\0','\0'};
        FILE *file;
        char *full_name;
        if( base_name == NULL ) {
            full_name = strdup( file_name );
        } else {
            asprintf( &full_name, "%s/%s", base_name, file_name );
        }
        if( (file = fopen(full_name, "r")) == NULL ) {
            squash_error( "Can't open %s to determine filetype", file_name );
        }
        fread( buf, 4, 1, file );
        if( (buf[0] == '\xFF' && (buf[1] & '\xE0') == '\xE0' ) ||
            (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3' && buf[3] == 3) ) {
            squash_log("Magically mp3-delicious");
            type = TYPE_MP3;
        } else if ( buf[0] == 'O' && buf[1] == 'g' && buf[2] == 'g' && buf[3] == 'S' ) {
            squash_log("Magically ogg-delicious");
            type = TYPE_OGG;
        } else if ( buf[0] == 'f' && buf[1] == 'L' && buf[2] == 'a' && buf[3] == 'C' ) {
            squash_log("Magically flac-delicious");
            type = TYPE_FLAC;
        } else {
            squash_log("Just ain't enough magic in the world");
        }
        fclose( file );
        squash_free( full_name );
    }
#endif

    /* Return the information */
    return type;
}

/*
 * Throw an error (i.e. take down the damn player)
 * NOTE: Private function, please use squash_error() instead
 */
void _squash_error( const char *filename, int line_num, const char *format, ... ) {
    char *error_message, *error_message_complete;

    /* Build the error message */
    error_message_complete = NULL;
    if( format != NULL ) {
        va_list printf_args;

        va_start( printf_args, format );
        vasprintf( &error_message, format, printf_args );
        va_end( printf_args );

        /* Buuild the final message */
        asprintf( &error_message_complete, "Error in '%s' at line %d: '%s'", filename, line_num, error_message );
        squash_free( error_message );
    }

#ifdef DEBUG
    /* Print log message */
    if( pthread_mutex_trylock(&log_info.lock) == 0 && log_info.log_file != NULL ) {
        if( error_message_complete != NULL) {
            fprintf( log_info.log_file, "%s\n", error_message_complete );
        }
        fclose( log_info.log_file );
    }
#endif

    /* Get the status lock.  It is imparitive that no one else hold this lock for long. */
    if( pthread_mutex_lock( &status_info.lock ) ) {
        exit( 2 );
    }

    /* Setup status information */
    status_info.status_message = error_message_complete;
    status_info.exit_status = 3;

    /* Release the status lock */
    if( pthread_mutex_unlock(&status_info.lock) ) {
        exit( 2 );
    }

    /* Send the signal.  Go Linux Threads!! */
    while( 1 ) {
        if( pthread_cond_broadcast(&status_info.exit) ) {
            exit( 2 );
        }
    }

    /* Kill the thread. If Linux threads worked, we would get here. */
    pthread_exit( (void *)NULL );
}

/*
 * Log an message to the log file.
 * NOTE: Private function, please use squash_log() instead.
 */
void _squash_log( const char *filename, int line_num, const char *format, ... ) {
#ifdef DEBUG
    /* Acquire log lock */
    if( pthread_mutex_lock(&log_info.lock) ) {
        squash_error( "Unable to lock log_info mutex" );
    }

    if( log_info.log_file != NULL ) {
        /* Print log message */
        if( format != NULL ) {
            va_list printf_args;

            va_start( printf_args, format );
            fprintf( log_info.log_file, "%s:%d - ", filename, line_num );
            vfprintf( log_info.log_file, format, printf_args );
            fprintf( log_info.log_file, "\n" );
            va_end( printf_args );
        }

        /* Flush the buffer */
        fflush( log_info.log_file );
    }

    /* Release log lock */
    if( pthread_mutex_unlock(&log_info.lock) ) {
        squash_error( "Unable to unlock log mutex" );
    }
#endif
}

/*
 * Parses a file for key value pairs.  Also supports header sections.
 * An example file looks like this:
[database]
readonly=0
 * These are the current restrictions:
 * Comments must start before any non-whitespace,
 * Headers may not have whitespace between the [] (otherwise that is captured too)
 * Keys and values have leading or trailing whitespace ignored.
 * Keys may not contain '=' nor start with '#' or '['
 * add_data() is called on on each key value pair found.
 * the void *data, char *header, char *key and char *value are add_data()'s parameter's.
 * You must make a copy of header if you want to have a copy, you must free
 * key and value if you do not want a copy.
 */
bool parse_file( const char *file_name, void(*add_data)(void*, char*, char*, char*), void *data ) {
    /* Position Variables */
    char *cur_pos = NULL;
    char *cur_start = NULL;
    char *cur_end = NULL;

    /* Meta-Data information */
    char *cur_header = NULL;
    char *cur_key = NULL;
    char *cur_value = NULL;

    /* State Information */
    enum state_list {
        STATE_IN_COMMENT,
        STATE_BEFORE_TOKEN,
        STATE_IN_HEADER,
        STATE_IN_KEY,
        STATE_BEFORE_VALUE,
        STATE_IN_VALUE
    } state;

    /* File Information */
    FILE *file_desc;
    struct stat file_info;
    char *file_data = NULL;
    bool exit = TRUE;

    /* Open the file */
    if( (file_desc = fopen(file_name, "r")) == NULL ) {
        /* Error opening the file */
        exit = FALSE;
        goto cleanup;
    }

    /* Get information on the file */
    if( fstat(fileno(file_desc), &file_info) ) {
        /* Error loading file information */
        exit = FALSE;
        goto cleanup;
    }

    /* Map the file into memory */
    if( (file_data = (char *)mmap( (void *)NULL, file_info.st_size, PROT_READ, MAP_SHARED, fileno(file_desc), 0)) == MAP_FAILED ) {
        /* Error mapping the file */
        exit = FALSE;
        goto cleanup;
    }

    /* Read the file */
    state = STATE_BEFORE_TOKEN;
    for( cur_pos = file_data; (cur_pos - file_data) < (file_info.st_size); cur_pos++ ) {
        switch( state ) {
            case STATE_IN_COMMENT:
                switch( *cur_pos ) {
                    case '\n':
                        state = STATE_BEFORE_TOKEN;
                        break;
                }
                break;
            case STATE_BEFORE_TOKEN:
                switch( *cur_pos ) {
                    case '#':
                        state = STATE_IN_COMMENT;
                        break;
                    case ' ':
                    case '\t':
                    case '\r':
                    case '\n':
                        /* Ignore whitespace */
                        break;
                    case '[':
                        state = STATE_IN_HEADER;
                        cur_start = cur_pos;
                        cur_end = cur_pos;
                        break;
                    default:
                        state = STATE_IN_KEY;

                        /* Set start position */
                        cur_start = cur_pos;
                        cur_end = cur_pos;
                }
                break;
            case STATE_IN_HEADER:
                switch( *cur_pos ) {
                    case ']':
                        state = STATE_BEFORE_TOKEN;

                        /* Save the header */
                        squash_free( cur_header );

                        cur_start++;

                        if( cur_start <= cur_end ) {
                            cur_header = copy_string( cur_start, cur_end );
                        }

                        break;
                    case '\r':
                    case '\n':
                        state = STATE_BEFORE_TOKEN;
                        break;
                    default:
                        cur_end = cur_pos;
                }
                break;
            case STATE_IN_KEY:
                switch( *cur_pos ) {
                    case '=':
                        state = STATE_BEFORE_VALUE;

                        /* Save the meta key */
                        cur_key = copy_string( cur_start, cur_end );

                        break;
                    case ' ':
                    case '\t':
                        /* Ignore whitespace */
                        break;
                    case '\r':
                    case '\n':
                        state = STATE_BEFORE_TOKEN;
                        break;
                    default:
                        cur_end = cur_pos;
                }
                break;
            case STATE_BEFORE_VALUE:
                switch( *cur_pos ) {
                    case ' ':
                    case '\t':
                        /* Ignore whitespace */
                        break;
                    case '\r':
                    case '\n':
                        state = STATE_BEFORE_TOKEN;

                        /* Add the meta data with an empty value */
                        add_data( data, cur_header, cur_key, NULL );
                        cur_key = NULL;
                        break;
                    default:
                        state = STATE_IN_VALUE;

                        /* Set start position */
                        cur_start = cur_pos;
                        cur_end = cur_pos;
                }
                break;
            case STATE_IN_VALUE:
                switch( *cur_pos ) {
                    case ' ':
                    case '\t':
                        /* Ignore whitespace */
                        break;
                    case '\r':
                    case '\n':
                        state = STATE_BEFORE_TOKEN;

                        /* Save the meta value */
                        cur_value = copy_string( cur_start, cur_end );

                        /* Add meta data to the database */
                        add_data( data, cur_header, cur_key, cur_value );
                        cur_key = NULL;
                        cur_value = NULL;
                        break;
                    default:
                        /* Guess at the ending */
                        cur_end = cur_pos;
                }
                break;
            default:
                break;
        }
    }
    switch( state ) {
        case STATE_BEFORE_VALUE:
            /* Add meta key to the database */
            add_data( data, cur_header, cur_key, NULL );
            cur_key = NULL;

            break;
        case STATE_IN_VALUE:
            /* Save the meta value */
            cur_value = copy_string( cur_start, cur_end );

            /* Add meta data to the database */
            add_data( data, cur_header, cur_key, cur_value );
            cur_key = NULL;
            cur_value = NULL;

            break;
        default:
            break;
    }
    squash_free( cur_header );

cleanup:
    /* Unmap the memory region */
    if( file_data != NULL ) {
        munmap( file_data, file_info.st_size );
        file_data = NULL;
    }

    /* Close the file descriptor */
    if( file_desc != NULL ) {
        fclose( file_desc );
        file_desc = NULL;
    }

    return exit;
}

/* Used by parse_file() to set config values read */
void set_config( void *data, char *header, char *key, char *value ) {
    int i;

    for( i = 0; i < CONFIG_KEY_COUNT; i++ ) {
        if( key && !strcasecmp(key, config_keys[i].key) && !(header && strcasecmp(header, config_keys[i].header)) ) {
            switch( config_keys[i].type )  {
                case TYPE_STRING:
                    *((char **)config_keys[i].value) = value;
                    break;
                case TYPE_INT:
                    *((int *)config_keys[i].value) = atoi( value );
                    squash_free( value );
                    break;
                case TYPE_DOUBLE:
                    *((double *)config_keys[i].value) = atof( value );
                    squash_free( value );
                    break;
            }
            squash_free( key );
        }
    }
}

/*
 * Expands a path name (tilde expansion, etc.).
 */
void expand_path( char **path ) {
#ifdef EMPEG
    /* This routine does a nice segfault on the empeg unit. */
    return;
#else
    wordexp_t wordexp_buffer;
    if( *path == NULL ) {
        return;
    }

    switch( wordexp(*path, &wordexp_buffer, 0) ) {
        case 0: /* no error */
            break;
        case WRDE_NOSPACE:
            /* error, but maybe some stuff got allocated so let's play nice */
            wordfree( &wordexp_buffer );
        default:
            squash_free( *path );
            return;
    }

    squash_free( *path );
    *path = strdup( wordexp_buffer.we_wordv[0] );
    wordfree( &wordexp_buffer );
#endif
}

/* Reads configuration values */
void init_config( void ) {
    char *config_path;

    /* Database Options */
    config.db_paths[ BASENAME_SONG ] = strdup(".");
    config.db_paths[ BASENAME_META ] = NULL;
    config.db_paths[ BASENAME_STAT ] = NULL;
    config.db_masterlist_path = NULL;
    config.db_readonly = 0;
    config.db_saveinfo = 1;
    config.db_overwriteinfo = 0;
    config.db_manual_rating_bias = 0.5;

    /* Control Options */
    config.global_state_path = strdup("~/.squash_state");
    config.input_fifo_path = strdup("~/.squash_control");

#ifdef EMPEG_DSP
    config.min_save_volume = 40;
    config.max_save_volume = 70;
#endif

    /* Playlist Manager Options */
    config.playlist_manager_playlist_size = 32;
    config.playlist_manager_pastlist_size = 32;

    /* Debug Options */
#ifdef DEBUG
    config.squash_log_path = strdup("~/.squash_log");
#endif

    /* Load Config files */

#ifdef EMPEG
    /* Load global squash.conf */
    parse_file( "/drive0/squash/squash.conf", set_config, NULL );
#else
    /* Load global squash.conf */
    parse_file( "/etc/squash.conf", set_config, NULL );

    /* Load user squash.conf */
    config_path = strdup("~/.squash");
    expand_path( &config_path );
    parse_file( config_path, set_config, NULL );
    free( config_path );
#endif

    /* Load environment specified squash.conf */
    if( (config_path = getenv("SQUASH_CONF")) != NULL ) {
        parse_file( config_path, set_config, NULL );
    }

    /* Expand Paths */
    expand_path( &config.global_state_path );
    expand_path( &config.input_fifo_path );
    expand_path( &config.db_paths[ BASENAME_SONG ] );
    expand_path( &config.db_paths[ BASENAME_META ] );
    expand_path( &config.db_paths[ BASENAME_STAT ] );
    expand_path( &config.db_masterlist_path );
#ifdef DEBUG
    expand_path( &config.squash_log_path );
#endif

#ifdef EMPEG_DSP
    if( config.min_save_volume < 0 ) {
        config.min_save_volume = 0;
    }
    if( config.max_save_volume > 100 ) {
        config.max_save_volume = 100;
    }
#endif

    /* Update any remaining defaults */
    if( config.db_paths[ BASENAME_META ] == NULL ) {
        config.db_paths[ BASENAME_META ] = strdup( config.db_paths[ BASENAME_SONG ] );
    }
    if( config.db_paths[ BASENAME_STAT ] == NULL ) {
        config.db_paths[ BASENAME_STAT ] = strdup( config.db_paths[ BASENAME_SONG ] );
    }
}

/*
 * Copies a string out of a larger string.
 */
char *copy_string( const char *start, const char *end ) {
    int length;
    char *new_string;

    /* Make sure we have valid pointers */
    if( (start == NULL) || (end == NULL) ) {
        return NULL;
    }

    /* Calculate string length */
    if( (length = end - start + 2) <= 0 ) {
        return NULL;
    }

    /* Create space for a copy of the string */
    squash_malloc( new_string, length);

    /* Create a copy of the string */
    memcpy( new_string, start, length - 1 );
    new_string[ length - 1 ] = '\0';

    /* Return the new string */
    return new_string;
}

/*
 * This combines the basename of a song with the song's filename
 */
char *build_fullfilename( song_info_t *song, enum basename_type_e type ) {
    int length;
    int ext;
    char *filename;

    if( song == NULL ) {
        return NULL;
    }

    length = strlen(song->basename[type]) + strlen(song->filename) + 2;
    if( type != BASENAME_SONG ) {
        /* TODO: fix this with a table */
        if( type == BASENAME_META ) {
            ext = TYPE_META;
        } else {
            ext = TYPE_STAT;
        }
        length += strlen(db_extensions[ext].extension) + 1;

        squash_malloc( filename, length);
        snprintf( filename, length, "%s/%s.%s", song->basename[type], song->filename, db_extensions[ext].extension );
    } else {
        squash_malloc( filename, length);
        snprintf( filename, length, "%s/%s", song->basename[type], song->filename );
    }

    return filename;
}

/*
 * creates a directory tree.  note that dir is temporarily modified, but it's
 * value is restored by the end of the routine.
 */
void create_path( char *dir ) {
    char *cur;
    int result;

    cur = dir;
    while( *cur != '\0' ) {
        if( *cur == '/' ) {
            if( cur - dir > 0 ) {
                *cur = '\0';

                result = mkdir( dir, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH );
                if( result != 0 && errno != EEXIST ) {
                    squash_error("Could not create info file directory '%s' (%d)", dir, errno );
                }
                *cur = '/';
            }
        }
        cur++;
    }

    result = mkdir( dir, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH );
    if( result != 0 && errno != EEXIST ) {
        squash_error("Could not create info file directory '%s' (%d)", dir, errno );
    }
}


