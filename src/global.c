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

/* File Extensions to use. */
const db_extension_info_t db_extensions[] = {
    {"info", BASENAME_META, insert_meta_data, NULL, NULL },
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
    { "Database", "Readonly", (void *)&config.db_readonly, TYPE_INT },
    { "Global", "Control_Filename", (void *)&config.input_fifo_path, TYPE_STRING },
#ifdef DEBUG
    { "Global", "Log_Filename", (void *)&config.squash_log_path, TYPE_STRING },
#endif
    { "Playlist", "Filename", (void *)&config.playlist_manager_playlist_path, TYPE_STRING },
    { "Playlist", "Size", (void *)&config.playlist_manager_playlist_size, TYPE_INT }
};


/*
 * Return the file format of the song (mp3, ogg, etc.).
 * TODO: Check using something smarter than just the filename
 */
enum song_type_e get_song_type( const char *file_name ) {
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
    } else  if( !strncasecmp(file_name + file_name_length - 4, ".ogg", 4) ) {
        type = TYPE_OGG;
    }

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
void parse_file( const char *file_name, void(*add_data)(void*, char*, char*, char*), void *data ) {
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

    /* Open the file */
    if( (file_desc = fopen(file_name, "r")) == NULL ) {
        /* Error opening the file */
        goto cleanup;
    }

    /* Get information on the file */
    if( fstat(fileno(file_desc), &file_info) ) {
        /* Error loading file information */
        goto cleanup;
    }

    /* Map the file into memory */
    if( (file_data = (char *)mmap( (void *)NULL, file_info.st_size, PROT_READ, MAP_SHARED, fileno(file_desc), 0)) == MAP_FAILED ) {
        /* Error mapping the file */
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
}

/* Reads configuration values */
void init_config( void ) {
    char *config_path;

    /* Database Options */
    config.db_paths[ BASENAME_SONG ] = strdup(".");
    config.db_paths[ BASENAME_META ] = NULL;
    config.db_paths[ BASENAME_STAT ] = NULL;
    config.db_readonly = 0;

    /* Control Options */
    config.input_fifo_path = strdup("~/.squash_control");

    /* Playlist Manager Options */
    config.playlist_manager_playlist_path = strdup("~/.squash_playlist");
    config.playlist_manager_playlist_size = 32;

    /* Debug Options */
#ifdef DEBUG
    config.squash_log_path = strdup("~/.squash_log");
#endif

    /* Load Config files */

    /* Load global squash.conf */
    parse_file( "/etc/squash.conf", set_config, NULL );

    /* Load user squash.conf */
    config_path = "~/.squash";
    expand_path( &config_path );
    parse_file( config_path, set_config, NULL );
    free( config_path );

    /* Load environment specified squash.conf */
    if( (config_path = getenv("SQUASH_CONF")) != NULL ) {
        parse_file( config_path, set_config, NULL );
    }

    /* Expand Paths */
    expand_path( &config.input_fifo_path );
    expand_path( &config.playlist_manager_playlist_path );
    expand_path( &config.db_paths[ BASENAME_SONG ] );
    expand_path( &config.db_paths[ BASENAME_META ] );
    expand_path( &config.db_paths[ BASENAME_STAT ] );
#ifdef DEBUG
    expand_path( &config.squash_log_path );
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

