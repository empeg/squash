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
 * global.h
 * Contains all shared structures and global data for squash
 */

#ifndef SQUASH_GLOBAL_H
#define SQUASH_GLOBAL_H

#define _GNU_SOURCE

/*
 * Global Includes
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#ifndef NO_FFTW
    #include <fftw.h>
#endif
#ifdef NO_NCURSES
    typedef unsigned char bool;
    typedef char chtype;
    #define FALSE 0
    #define TRUE 1
#else
    #include <curses.h>
#endif
#ifdef EMPEG_DSP
    #define SOUND_LITTLE 0
    #define SOUND_BIG 1
#else
    #include <ao/ao.h>
    #define SOUND_LITTLE AO_FMT_LITTLE
    #define SOUND_BIG AO_FMT_BIG
#endif
#include <math.h>
#include <stdarg.h>
#include <wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <wordexp.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

/*
 * Definitions
 */
#ifdef DEBUG
#ifdef EMPEG_DSP
    #define CONFIG_KEY_COUNT 15
#else
    #define CONFIG_KEY_COUNT 13
#endif
#else
#ifdef EMPEG_DSP
    #define CONFIG_KEY_COUNT 14
#else
    #define CONFIG_KEY_COUNT 12
#endif
#endif

/*
 * Enumerations
 */
enum basename_type_e { BASENAME_SONG, BASENAME_META, BASENAME_STAT };
enum song_type_e { TYPE_UNKNOWN, TYPE_OGG, TYPE_MP3, TYPE_FLAC };
enum system_state_e { SYSTEM_LOADING, SYSTEM_RUNNING };
enum data_type_e { TYPE_STRING, TYPE_INT, TYPE_DOUBLE };
enum meta_type_e { TYPE_META, TYPE_STAT }; /* these match with db_extensions array */

#ifdef EMPEG
#ifdef ADVENTURE
enum empeg_screen_e {
    EMPEG_SCREEN_NAVIGATION_1,
        EMPEG_SCREEN_PLAY,
            EMPEG_SCREEN_PLAY_SUBMENU,
                /* PLAY/PAUSE, */
                EMPEG_SCREEN_PLAY_SONG_INFO,
        EMPEG_SCREEN_PLAYLIST,
            EMPEG_SCREEN_PLAYLIST_SUBMENU,
                /* DELETE SONG, */
                EMPEG_SCREEN_PLAYLIST_SONG_INFO,
    EMPEG_SCREEN_NAVIGATION_2,
        EMPEG_SCREEN_QUIT,
        EMPEG_SCREEN_DELETE_MASTERLIST,
        EMPEG_SCREEN_SET_DISPLAY_BRIGHTNESS,
    EMPEG_SCREEN_NONE
};
#else
enum empeg_screen_e {
    EMPEG_SCREEN_MAIN_MENU,
        EMPEG_SCREEN_PLAY,
            EMPEG_SCREEN_PLAY_SONG_INFO,
            EMPEG_SCREEN_PLAY_RATE_SONG,
        EMPEG_SCREEN_PLAYLIST,
            EMPEG_SCREEN_PLAYLIST_SONG_INFO,
            EMPEG_SCREEN_PLAYLIST_RATE_SONG,
        EMPEG_SCREEN_GLOBAL_SONG_INFO,
        EMPEG_SCREEN_QUIT,
        EMPEG_SCREEN_SETTINGS,
            EMPEG_SCREEN_SET_DISPLAY_BRIGHTNESS,
            EMPEG_SCREEN_SET_RATING_BIAS,
    EMPEG_SCREEN_COUNT
};
#endif
#endif

/*
 * The difference between STOP and BIG_STOP is that BIG_STOP makes the player
 * not load the file before pausing.
 */
enum player_state_e { STATE_PLAY, STATE_PAUSE, STATE_STOP, STATE_BIG_STOP };
enum player_command_e { CMD_NOOP, CMD_PLAY, CMD_PAUSE, CMD_STOP, CMD_SKIP };

#ifdef NO_NCURSES
#else
enum window_states_e {
    WIN_STATE_NORMAL,
    WIN_STATE_HIDDEN,
    WIN_STATE_MAXIMIZED
};

enum colors_e { 
    TEXT_BLACK,
    TEXT_GREEN,
    TEXT_RED,
    TEXT_CYAN,
    TEXT_WHITE,
    TEXT_MAGENTA,
    TEXT_BLUE,
    TEXT_YELLOW,
    TEXT_BLUE_SELECTED,
    TEXT_BLUE_BACKGROUND
};

enum windows_e {
    WIN_NOW_PLAYING,
    WIN_INFO,
    WIN_SPECTRUM,
    WIN_PASTLIST,
    WIN_PLAYLIST,
    WIN_HELP,
    WIN_COUNT /* faux entry, not a real window */
};
#endif

/*
 * Structures
 */
/* Configuration */
typedef struct config_keys_s {
    char *header;
    char *key;
    void *value;
    enum data_type_e type;
    char *default_value;
} config_keys_t;

typedef struct config_s {
    char *db_paths[3];
    char *db_masterlist_path;
    int db_readonly;
    int db_saveinfo;
    int db_overwriteinfo;
    float db_manual_rating_bias;

    char *global_state_path;
    char *input_fifo_path;

    int playlist_manager_playlist_size;
    int playlist_manager_pastlist_size;

#ifdef EMPEG_DSP
    int min_save_volume;
    int max_save_volume;
#endif

#ifdef DEBUG
    char *squash_log_path;
#endif
} config_t;

/* Database */
typedef struct meta_key_s {
    char *key;
    char **values;
    int value_count;
} meta_key_t;

typedef struct stat_info_s {
    int play_count;
    int skip_count;
    int repeat_counter;
    int manual_rating;
    bool changed;
} stat_info_t;

typedef struct song_info_s {
    char *basename[3];
    char *filename;

    meta_key_t *meta_keys;
    int meta_key_count;

    stat_info_t stat;

    long play_length; /* milliseconds */
    enum song_type_e song_type;
} song_info_t;

typedef struct key_set_s {
    char **keys;
    int key_count;
} key_set_t;

typedef struct db_search_result_s {
    song_info_t **songs;
    int song_count;
} db_search_result_t;

typedef struct db_extension_info_s {
    char *extension;
    int which_basename;
    void(*add_data)(void*, char*, char*, char*);
    void(*save_data)(song_info_t*, FILE*);
    bool(*is_changed)(song_info_t*);
} db_extension_info_t;

typedef struct database_info_s {
    pthread_rwlock_t lock;
    song_info_t *songs;
    int song_count;
    int song_count_allocated;
    bool stats_loaded;
    pthread_cond_t stats_finished;
    double sum;
    double sqr_sum;
    long play_sum;
    long play_sqr_sum;
    long skip_sum;
    long skip_sqr_sum;
} database_info_t;

/* Sound device structures */
#ifdef EMPEG_DSP
typedef struct sound_device_s {
    int audio;
    int mixer;
    char *buffer;
    int buffer_size;
    int volume[2];
} sound_device_t;
typedef struct sound_format_s {
/* While these are present, the empeg dsp code will not work
 * unless rate is 44100, channels is 2, byte_format is little,
 * and bits is 16.  This will be fixed later. */
    int rate;
    int channels;
    int byte_format;
    int bits;
} sound_format_t;
#else
typedef ao_device sound_device_t;
typedef ao_sample_format sound_format_t;
#endif

/* Playback Structures */
typedef struct frame_data_s {
    char *pcm_data;
    int pcm_size;
    long position; /* milliseconds */
} frame_data_t;

typedef struct song_functions_s {
    void *(*open)( char *filename, sound_format_t *format );
    frame_data_t(*decode_frame)( void * );
    long(*calc_duration)( void * );
    void(*seek)( void *, long, long );
    void(*close)( void * );
} song_functions_t;

/* Playlist Structures */
typedef struct state_raw_song_s {
    char *filename;
    long position;
} state_raw_song_t;

typedef struct state_info_s {
    pthread_mutex_t lock;
    state_raw_song_t *raw_songs;
    long raw_song_count;
    long raw_song_alloc;
    long current_song;
#ifndef NO_NCURSES
    int window_states[WIN_COUNT];
#endif
    int cur_window_number;
    int volume;
    int brightness;
} state_info_t;

typedef struct song_queue_entry_s {
    struct song_queue_entry_s *next;
    song_info_t *song_info;
    long start_position;
} song_queue_entry_t;

typedef struct song_queue_s {
    pthread_mutex_t lock;
    pthread_cond_t    not_empty;
    pthread_cond_t    not_full;
    song_queue_entry_t *head;
    song_queue_entry_t *tail;
    int size;
    int wanted_size;
    int selected;
} song_queue_t;

/* Status Information */
typedef struct player_command_entry_s {
    struct player_command_entry_s *next;
    enum player_command_e command;
} player_command_entry_t;

typedef struct player_command_s {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    player_command_entry_t *head;
    player_command_entry_t *tail;
    int size;
} player_command_t;

typedef struct player_info_s {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    song_info_t *song;
    long current_position;
    enum player_state_e state;
    sound_device_t *device;
} player_info_t;

typedef struct frame_buffer_s {
    pthread_mutex_t lock;
    pthread_cond_t restart;
    pthread_cond_t new_data;
    frame_data_t *frames;
    int size;
    int pcm_size;
    bool song_eof;
    bool new_file;
    void *decoder_data;
    frame_data_t(* decoder_function)( void * );
    void(*close_function)( void * );
} frame_buffer_t;

typedef struct status_info_s {
    pthread_mutex_t lock;
    pthread_cond_t exit;
    char *status_message;
    int exit_status;
} status_info_t;

#ifndef NO_NCURSES
/* Window Info */
typedef struct win_info_s {
    WINDOW *window;
    union {
        struct {
            int height;
        } fixed;
        struct {
            int height;
            int weight;
            int min_height;
        } calc;
    } size;
    enum window_states_e state;
    unsigned int is_fixed : 1;
    unsigned int is_persistent : 1;
} win_info_t;
#endif

typedef struct menu_item_s {
    char *title;
    int indentation;
    int screen_on_press;
    bool leave_menu;
} menu_item_t;

typedef struct menu_list_s {
    menu_item_t *items;
    int item_count;
    int default_item;
} menu_list_t;

typedef struct display_info_s {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    enum system_state_e state;
#ifdef EMPEG
    int screen_fd;
    char *screen;
#ifdef ADVENTURE
    enum empeg_screen_e cur_screen;
#else
    enum empeg_screen_e cur_screen;
    bool in_menu;
    int menu_selection;
#endif
    int brightness;
#endif
#ifndef NO_NCURSES
    win_info_t window[ WIN_COUNT ];
    int focus;
    bool too_small;
#endif
} display_info_t;

/* Log information */
#ifdef DEBUG
typedef struct log_info_s {
    pthread_mutex_t lock;
    FILE *log_file;
} log_info_t;
#endif

/* Spectrum Structures */
typedef struct spectrum_ring_s {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    bool active;
    long head;
    long tail;
    char *ring;
} spectrum_ring_t;

typedef struct spectrum_info_s {
    pthread_mutex_t lock;
    double *window;
    sound_format_t sound_format;
#ifndef NO_FFTW
    fftw_complex *in;
    fftw_complex *out;
    fftw_plan plan;
#endif
    chtype *bitmap;
    double *bar_heights;
    double *cap_heights;
    int *bar_widths;
    int width;
    int height;
} spectrum_info_t;

/*
 * Shared (Global) Variables
 */
display_info_t display_info;
song_queue_t song_queue;
song_queue_t past_queue;
player_command_t player_command;
player_info_t player_info;
frame_buffer_t frame_buffer;
status_info_t status_info;
spectrum_ring_t spectrum_ring;
spectrum_info_t spectrum_info;
database_info_t database_info;
state_info_t state_info;

/* File Extensions to check, values defined at top of global.c */
extern const db_extension_info_t db_extensions[];
extern const int db_extensions_size;

config_t config;
/* Holds valid configuration varables, defined at the top of global.c */
extern config_keys_t config_keys[ CONFIG_KEY_COUNT ];

#ifdef EMPEG
/* Holds the menu structure for the empeg */
extern menu_list_t menu_list[ EMPEG_SCREEN_COUNT ];
#endif

#ifdef DEBUG
log_info_t log_info;
#endif

/*
 * Macros -- Most of these may not be used unless you are in a thread!
 */
#define squash_error( ... ) _squash_error( __FILE__, __LINE__, __VA_ARGS__ )
#ifdef DEBUG
    #define squash_log( ... ) _squash_log( __FILE__, __LINE__, __VA_ARGS__ )
#else
    #define squash_log( ... )
#endif

#define squash_malloc( ptr, size ) if( (ptr = malloc(size)) == NULL ) squash_error( "Unable to allocate memory for " #ptr )
#define squash_calloc( ptr, nmemb, size ) if( (ptr = calloc(nmemb, size)) == NULL ) squash_error( "Unable to allocate memory for " #ptr )
#define squash_realloc( ptr, size ) if( (ptr = realloc(ptr, size)) == NULL ) squash_error( "Unable to re-allocate memory for " #ptr )
#define squash_free( ptr ) ((ptr) == NULL) ? 0 : ( free(ptr), (ptr) = NULL )

#ifdef LOCK_DEBUG
    #define LOCK_LOG(a,b) squash_log("lock debug: %s %s", a, #b);
#else
    #define LOCK_LOG(a,b)
#endif

#define squash_lock( mutex ) LOCK_LOG("lock", mutex) if( pthread_mutex_lock(&(mutex)) ) squash_error( "Unable to lock " #mutex " mutex" )
#define squash_unlock( mutex ) LOCK_LOG("unlock", mutex) if( pthread_mutex_unlock(&(mutex)) ) squash_error( "Unable to unlock " #mutex " mutex" )
#define squash_rlock( mutex ) LOCK_LOG("rlock", mutex) if( pthread_rwlock_rdlock(&(mutex)) ) squash_error( "Unable to lock " #mutex " mutex" )
#define squash_runlock( mutex ) LOCK_LOG("runlock", mutex) if( pthread_rwlock_unlock(&(mutex)) ) squash_error( "Unable to unlock " #mutex " mutex" )
#define squash_wlock( mutex ) LOCK_LOG("wlock", mutex) if( pthread_rwlock_wrlock(&(mutex)) ) squash_error( "Unable to lock " #mutex " mutex" )
#define squash_wunlock( mutex ) LOCK_LOG("wunlock", mutex) if( pthread_rwlock_unlock(&(mutex)) ) squash_error( "Unable to unlock " #mutex " mutex" )
#define squash_wait( cond, mutex ) LOCK_LOG("wait on", cond) if( pthread_cond_wait(&(cond), &(mutex)) ) squash_error( "Unable to wait for " #cond " on mutex " #mutex )
#define squash_signal( cond ) LOCK_LOG("signal", cond) if( pthread_cond_signal(&(cond)) ) squash_error( "Unable to signal " #cond " condition" )
#define squash_broadcast( cond ) LOCK_LOG("broadcast", cond) if( pthread_cond_broadcast(&(cond)) ) squash_error( "Unable to broadcast " #cond " condition" )

#define squash_ensure_alloc( ensure, current, ptr, ptr_size, initial, increment ) \
    { \
        if( ensure >= current ) { \
            if( ensure == 0 ) { \
                current = initial; \
            } else { \
                current increment; \
            } \
            squash_realloc( ptr, current * ptr_size ); \
        } \
    }

#define squash_asprintf( dest, format, ... ) if( asprintf( &dest, format, __VA_ARGS__ ) == -1 ) squash_error( "Unable to allocate memory for string %s", #dest )

/*
 * Prototypes
 */
void save_state();
enum song_type_e get_song_type( const char *base_name, const char *file_name );
void _squash_error( const char *filename, int line_num, const char *format, ... );
void _squash_log( const char *filename, int line_num, const char *format, ... );
bool parse_file( const char *file_name, void(*add_data)(void*, char*, char*, char*), void *data );
void set_config( void *data, char *header, char *key, char *value );
void init_config( void );
char *copy_string( const char *start, const char *end );
char *build_fullfilename( song_info_t *song, enum basename_type_e type );
void create_path( char *dir );

#endif
