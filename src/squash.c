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
 * squash.c
 */
#include "global.h"
#include "global_squash.h"
#include "player.h"             /* for player() */
#include "playlist_manager.h"   /* for playlist_manager() */
#include "database.h"           /* for load_meta_data() etc. */
#include "stat.h"               /* for start_song_picker() */
#include "display.h"            /* for display_monitor() */
#include "input.h"              /* for keyboard_monitor(), fifo_monitor() */
#include "spectrum.h"           /* for spectrum_monitor() */
#include "sound.h"              /* for sound_init() sound_shutdown() */
#ifdef EMPEG
#include "vfdlib.h"             /* for exit status display */
#include <sys/ioctl.h>          /* for ioctl() */
#endif
#include "squash.h"

#ifdef EMPEG
void *power(void *data) {
    int power_fd;
    int power_flags;

    if ((power_fd = open("/dev/empeg_power", O_RDWR)) == -1) {
        squash_log("Couldn't open power device\n");
        return (void *)NULL;
    }

    ioctl( power_fd, _IOR('p', 2, int), &power_flags );

    if( power_flags & 0x01 ) {
        while( power_flags & 0x04 ) {
            sleep(1);
            ioctl( power_fd, _IOR('p', 2, int), &power_flags );
        }
        close( power_fd );

        /* Grab the status lock */
        squash_lock( status_info.lock );

        /* Setup status information */
        status_info.status_message = "Power fail";
        status_info.exit_status = 0;

        /* Release the status lock */
        squash_unlock( status_info.lock );

        /* Signal the main thread */
        squash_signal( status_info.exit );
    } else {
        close( power_fd );
    }

    return (void *)NULL;
}
#endif

void *state_saver(void *data) {
    struct timespec sleep_time = { 120, 000000000 };
    while(1) {
        nanosleep( &sleep_time, NULL );
        squash_lock( state_info.lock );
        squash_lock( display_info.lock );
        squash_rlock( database_info.lock );
        squash_lock( song_queue.lock );
        squash_lock( player_info.lock );
        save_state();
        squash_unlock( player_info.lock );
        squash_unlock( song_queue.lock );
        squash_runlock( database_info.lock );
        squash_unlock( display_info.lock );
        squash_unlock( state_info.lock );
    }

    return (void *)NULL;
}

#ifndef NO_NCURSES
void *screen_redraw(void *data) {
    struct timespec sleep_time = { 1, 000000000 };
    while(1) {
        nanosleep( &sleep_time, NULL );
        squash_lock( display_info.lock );
        if( display_info.too_small ) {
            draw_screen();
        }
        squash_unlock( display_info.lock );
    }

    return (void *)NULL;
}
#endif

/*
 * This is called by main() to load the DB.  Since nothing else can happen until
 * the database is loaded, it does a few other things, in order to tell the other
 * threads that the database has loaded, and that they can start.
 */
void *setup_database( void *data ) {
    /* Grab our locks */
    squash_lock( display_info.lock );
    squash_wlock( database_info.lock );

    /* Load the database */
    load_db_filenames();

    /* Tell the rest of the system we are done loading the file list */
    display_info.state = SYSTEM_RUNNING;

    /* Release our locks */
    squash_wunlock( database_info.lock );
    squash_unlock( display_info.lock );

    squash_log("starting spectrum");
    /* Tell the specturm analyzer it can run */
    squash_lock( spectrum_ring.lock );
    spectrum_ring.active = TRUE;
    squash_unlock( spectrum_ring.lock );

    squash_log("starting player");
    /* Tell the player to start playing */
    squash_lock( player_command.lock );
    player_queue_command( CMD_PLAY );
    squash_signal( player_command.changed );
    squash_unlock( player_command.lock );

    squash_log("loading state");
    /* Load any previous playing song */
    load_state();

    squash_log("drawing screen");
    draw_screen();
#ifdef EMPEG
    draw_empeg_display();
#endif

    squash_log("starting playlist manager");
    /* Tell the playlist_manager() thread to add songs */
    squash_lock( song_queue.lock );
    song_queue.wanted_size = config.playlist_manager_playlist_size;
    squash_signal( song_queue.not_full );
    squash_unlock( song_queue.lock );

    squash_log("loading stats");
    load_all_meta_data( TYPE_STAT ); /* Load statistics */
    /* Load the statistics routine (needed for playlist_manager() to call pick_song()) */
    start_song_picker();
    squash_signal( database_info.stats_finished );
    squash_log("stats loaded, database thread ending");

    /* This isn't necessary anymore, since we load metadata on playlist load instead.
     * (Will be needed again once searching has been added). */
#if 0
    load_all_meta_data( TYPE_META ); /* Load info files */
    squash_log("metadata loaded");
#endif
    return (void *)NULL;
}

/*
 * Program entry point
 */
int main( int argc, char *argv[] ) {
    pthread_t display_thread;
#ifdef EMPEG
    pthread_t ir_input_thread;
    pthread_t power_thread;
#endif
#ifndef NO_NCURSES
    pthread_t keyboard_input_thread;
    pthread_t screen_redraw_thread;
#endif
    pthread_t fifo_input_thread;
    pthread_t player_thread;
    pthread_t frame_decoder_thread;
    pthread_t playlist_manager_thread;
#ifndef EMPEG
    pthread_t spectrum_thread;
#endif
    pthread_t state_saver_thread;
    pthread_t database_thread;
    pthread_attr_t thread_attr;
#ifdef EMPEG
    struct sched_param thread_sched_param;
    int thread_policy;
    int low_priority, medium_priority, high_priority;
#endif
    struct stat fifo_stat;
    int fifo_stat_result;

    init_config();

    /* Setup the log file */
#ifdef DEBUG
    #ifdef EMPEG
        /* for some reason the empeg won't keep this file descriptor open for the other threads
         * so we just use stderr which does stay open
         */
        log_info.log_file = stderr;
    #else
        log_info.log_file = fopen( config.squash_log_path, "w" );
    #endif
    squash_log( "Log initialized" );
#endif

   /* TODO: Move this it's own procedure.  What should be
     * done about two squash's running at once?  Perhaps
     * pid should be added to the name.  Also, unlink any
     * fifo's found that are of pid's that don't exist anymore.
     */
    fifo_stat_result = stat(config.input_fifo_path, &fifo_stat);
    if( fifo_stat_result == 0 ) {
        /* File already exists */
        if(! S_ISFIFO(fifo_stat.st_mode) ) {
            fprintf(stderr, "Sorry '%s' already exists, but isn't a fifo", config.input_fifo_path);
            exit(1);
        } else {
            if( unlink(config.input_fifo_path) != 0 ) {
                fprintf(stderr, "Couldn't remove existing '%s'", config.input_fifo_path);
                exit(1);
            }
        }
    }
    if( mkfifo( config.input_fifo_path, 0666 ) < 0 ) {
        fprintf(stderr, "Can't create '%s'\n", config.input_fifo_path);
        exit(1);
    }

    /* Initilize the display */
    display_init();
    display_info.state = SYSTEM_LOADING;
    /* Set focus. */
#ifndef NO_NCURSES
    display_info.focus = WIN_NOW_PLAYING;
#endif
#ifdef EMPEG
#ifdef ADVENTURE
    display_info.cur_screen = EMPEG_SCREEN_PLAY;
#else
    display_info.cur_screen = EMPEG_SCREEN_PLAY;
    display_info.in_menu = FALSE;
    display_info.menu_selection = 0;
#endif
#endif

    /* Initialize the windows, unless there are no windows */
#ifndef NO_NCURSES
    window_init();
#endif

    /* Initialize the spectrum analyzer */
    spectrum_init();

    /* Setup exit strategy */
    /* Commenting out the ignoring of ctrl-c, since
       we cannot handle key press events while loading
       the database (so there is no way to quit except
       uncleanly anyway (i.e. kill), might as well keep
       ctrl-c since people expect it to be here.
    (void)signal( SIGINT, SIG_IGN );
    */
    status_info.exit_status = -1;
    status_info.status_message = NULL;

    /* Initialize Database */
    database_info.song_count = 0;
    database_info.song_count_allocated = 0;
    database_info.songs = NULL;
    database_info.stats_loaded = FALSE;

    /* Initialize Song Queue */
    song_queue.head = NULL;
    song_queue.tail = NULL;
    song_queue.selected = 0;
    song_queue.size = 0;
    song_queue.wanted_size = 0; /* set by database startup thread */

    /* Initialize State Info */
    state_info.raw_songs = NULL;
    state_info.current_song = 0;
    state_info.raw_song_count = 0;
    state_info.raw_song_alloc = 0;
    state_info.cur_window_number = 0;
    state_info.volume = 100;
    state_info.brightness = 100;

    /* Initialize Past Queue */
    past_queue.head = NULL;
    past_queue.tail = NULL;
    past_queue.selected = 0;
    past_queue.size = 0;
    past_queue.wanted_size = config.playlist_manager_pastlist_size;

    /* Initialize the now playing information */
    player_command.head = NULL;
    player_command.tail = NULL;
    player_command.size = 0;
    player_info.state = STATE_BIG_STOP;
    player_info.song = NULL;
    player_info.current_position = 0;
    squash_malloc( frame_buffer.frames, PLAYER_MAX_BUFFER_SIZE * sizeof( frame_data_t ) );
    frame_buffer.song_eof = FALSE;
    frame_buffer.new_file = FALSE;
    frame_buffer.size = 0;
    frame_buffer.pcm_size = 0;
    frame_buffer.decoder_function = NULL;
    frame_buffer.decoder_data = NULL;

    /* Initialize the audio device */
    sound_init();

    /* Draw the screen */
    draw_screen();

    squash_log("starting threads...");
    pthread_attr_init( &thread_attr );
    pthread_attr_setschedpolicy( &thread_attr, SCHED_OTHER );
    /* Find out the scheduling values if we are on the empeg, so that we can use realtime mode */
#ifdef EMPEG
    pthread_attr_setschedpolicy( &thread_attr, SCHED_FIFO );
    pthread_attr_getschedpolicy( &thread_attr, &thread_policy );
    squash_log("policy %d, other %d, rr %d, fifo %d", thread_policy, SCHED_OTHER, SCHED_RR, SCHED_FIFO );
    pthread_getschedparam( 0, NULL, &thread_sched_param );
    squash_log("Max priority = %d", sched_get_priority_max(thread_policy) );
    squash_log("Min priority = %d", sched_get_priority_min(thread_policy) );
    high_priority = sched_get_priority_max(thread_policy);
    low_priority = sched_get_priority_min(thread_policy);
    medium_priority = (high_priority+low_priority)/2;

    /* Start support threads */
    thread_sched_param.sched_priority = medium_priority;
    pthread_attr_setschedparam( &thread_attr, &thread_sched_param );
    pthread_attr_setschedpolicy( &thread_attr, SCHED_FIFO );
#endif

    squash_log("starting display");
    pthread_create( &display_thread, &thread_attr, display_monitor, (void *)NULL );

#ifdef EMPEG
    thread_sched_param.sched_priority = low_priority;
    pthread_attr_setschedparam( &thread_attr, &thread_sched_param );
    pthread_attr_setschedpolicy( &thread_attr, SCHED_FIFO );
#endif

    squash_log("starting database");
    pthread_create( &database_thread, &thread_attr, setup_database, (void *)NULL );
    squash_log("starting playlist");
    pthread_create( &playlist_manager_thread, &thread_attr, playlist_manager, (void *)NULL );

    /* trying to display this is a waste right now on the empeg */
#ifndef EMPEG
    squash_log("starting spectrum");
    pthread_create( &spectrum_thread, &thread_attr, spectrum_monitor, (void *)NULL );
#endif

#ifdef EMPEG
    pthread_attr_setschedpolicy( &thread_attr, SCHED_FIFO );
    thread_sched_param.sched_priority = high_priority-1;
    pthread_attr_setschedparam( &thread_attr, &thread_sched_param );
#endif
    /* Start playing */
    squash_log("starting frame decoder");
    pthread_create( &frame_decoder_thread, &thread_attr, frame_decoder, (void *)NULL );

#ifdef EMPEG
    pthread_attr_setschedpolicy( &thread_attr, SCHED_FIFO );
    thread_sched_param.sched_priority = high_priority;
    pthread_attr_setschedparam( &thread_attr, &thread_sched_param );
#endif
    squash_log("starting player");
    pthread_create( &player_thread, &thread_attr, player, (void *)NULL );

#ifdef EMPEG
    thread_sched_param.sched_priority = medium_priority;
    pthread_attr_setschedparam( &thread_attr, &thread_sched_param );
    pthread_attr_setschedpolicy( &thread_attr, SCHED_FIFO );
#endif

    squash_log("starting fifo");
    pthread_create( &fifo_input_thread, &thread_attr, fifo_monitor, (void *)NULL );

#ifdef EMPEG
    /* Listen for input */
    squash_log("starting ir");
    pthread_create( &ir_input_thread, &thread_attr, ir_monitor, (void *)NULL );

    thread_sched_param.sched_priority = low_priority+1;
    pthread_attr_setschedparam( &thread_attr, &thread_sched_param );
    pthread_attr_setschedpolicy( &thread_attr, SCHED_FIFO );

    squash_log("starting power");
    pthread_create( &power_thread, &thread_attr, power, (void *)NULL );
#endif

    squash_log("starting state saver");
    pthread_create( &state_saver_thread, &thread_attr, state_saver, (void *)NULL );

#ifndef NO_NCURSES
    squash_log("starting keyboard");
    pthread_create( &keyboard_input_thread, &thread_attr, keyboard_monitor, (void *)NULL );
    squash_log("starting screen redraw");
    pthread_create( &screen_redraw_thread, &thread_attr, screen_redraw, (void *)NULL );
#endif

    squash_log("threads started, waiting till done");

    /* Grab the status lock */
    squash_lock( status_info.lock );

    while( status_info.exit_status == -1 ) {
        /* Wait for someone to signal a end condition */
        squash_wait( status_info.exit, status_info.lock );
    }

    /* Kill all threads */
#ifdef EMPEG
    pthread_cancel( ir_input_thread );
    pthread_cancel( power_thread );
#endif
#ifndef NO_NCURSES
    pthread_cancel( keyboard_input_thread );
    pthread_cancel( screen_redraw_thread );
#endif
    pthread_cancel( fifo_input_thread );
    pthread_cancel( display_thread );
#ifndef EMPEG
    pthread_cancel( spectrum_thread );
#endif
    pthread_cancel( playlist_manager_thread );
    pthread_cancel( player_thread );
    pthread_cancel( frame_decoder_thread );
    pthread_cancel( state_saver_thread );

    /* Save the state */
    squash_lock( state_info.lock );
    save_state();
    squash_unlock( state_info.lock );

    /* Bring ncurses down, unless there is no ncurses */
#ifndef NO_NCURSES
    /* Shut down ncurses */
    endwin();

    /* Reset the screen (some more) */
    reset_shell_mode();
#endif

    /* Shutdown the sound device */
    sound_shutdown();

    if( fifo_info.fifo_file != NULL ) {
        fclose( fifo_info.fifo_file );
    }
    unlink( config.input_fifo_path );

    /* Print any error information */
    if( status_info.status_message != NULL ) {
        printf( "%s\n", status_info.status_message );
    }

#ifdef EMPEG
    vfdlib_clear( display_info.screen, 0 );
    if( status_info.status_message == NULL ) {
        vfdlib_drawText( display_info.screen, "Squash Ended", 0, 0, 2, 3);
    } else {
        vfdlib_drawText( display_info.screen, status_info.status_message, 0, 0, 2, 3 );
    }
    ioctl(display_info.screen_fd, _IO('d', 0));
    sleep(1);

    /* Turn off the display */
    ioctl(display_info.screen_fd, _IOW('d', 1, int), 0);
#endif

    return status_info.exit_status;
}
