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
#include "player.h"             /* for player() */
#include "playlist_manager.h"   /* for playlist_manager() */
#include "database.h"           /* for setup_database() */
#include "display.h"            /* for display_monitor() */
#include "input.h"              /* for keyboard_monitor(), fifo_monitor() */
#include "spectrum.h"           /* for spectrum_monitor() */
#include "squash.h"

/*
 * Program entry point
 */
int main( int argc, char *argv[] ) {
    pthread_t display_thread;
    pthread_t keyboard_input_thread;
    pthread_t fifo_input_thread;
    pthread_t player_thread;
    pthread_t playlist_manager_thread;
    pthread_t spectrum_thread;
    pthread_t database_thread;
    struct stat fifo_stat;
    int fifo_stat_result;

    init_config();

    /* Setup the log file */
#ifdef DEBUG
    /* Open the log file */
    log_info.log_file = fopen( config.squash_log_path, "w" );
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

    /* Initialize the windows */
    window_init();

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
    status_info.exit_status = 0;
    status_info.status_message = NULL;

    /* Initialize Database */
    database_info.song_count = 0;
    database_info.song_count_allocated = 0;
    database_info.songs = NULL;
    database_info.stats_loaded = FALSE;

    /* Initialize Song Queue */
    song_queue.head = NULL;
    song_queue.tail = NULL;
    song_queue.selected = -1;
    song_queue.size = 0;
    song_queue.wanted_size = 0;

    /* Initialize the now playing information */
    player_command.head = NULL;
    player_command.tail = NULL;
    player_command.size = 0;
    player_info.state = STATE_BIG_STOP;
    player_info.song = NULL;
    player_info.duration = 0;
    player_info.current_position = 0;

    /* Draw the screen */
    draw_screen();

    /* Start support threads */
    pthread_create( &database_thread, NULL, setup_database, (void *)NULL );
    pthread_create( &playlist_manager_thread, NULL, playlist_manager, (void *)NULL );
    pthread_create( &display_thread, NULL, display_monitor, (void *)NULL );
    pthread_create( &spectrum_thread, NULL, spectrum_monitor, (void *)NULL );

    /* Start playing */
    pthread_create( &player_thread, NULL, player, (void *)NULL );

    /* Listen for input */
    pthread_create( &keyboard_input_thread, NULL, keyboard_monitor, (void *)NULL );
    pthread_create( &fifo_input_thread, NULL, fifo_monitor, (void *)NULL );

    /* Grab the status lock */
    squash_lock( status_info.lock );

    /* Wait for someone to signal a end condition */
    squash_wait( status_info.exit, status_info.lock );

    /* Kill all threads */
    pthread_cancel( keyboard_input_thread );
    pthread_cancel( fifo_input_thread );
    pthread_cancel( display_thread );
    pthread_cancel( spectrum_thread );
    pthread_cancel( playlist_manager_thread );
    pthread_cancel( player_thread );

    /* Shut down ncurses */
    endwin();

    /* Reset the screen (some more) */
    reset_shell_mode();

    if( fifo_info.fifo_file != NULL ) {
        fclose( fifo_info.fifo_file );
    }
    unlink( config.input_fifo_path );

    /* Print any error information */
    if( status_info.status_message != NULL ) {
        printf( "%s\n", status_info.status_message );
    }

    return status_info.exit_status;
}
