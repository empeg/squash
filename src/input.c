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
 * input.c
 */

#include "global.h"
#include "display.h"    /* for draw_screen() */
#include "player.h"     /* for player_queue_command() */
#include "database.h"   /* for clear_song_meta() and load_meta_data() */
#include "stat.h"       /* for feedback() */
#include "input.h"

/*
 * Monitor keyboard events
 */
void *keyboard_monitor( void *input_data ) {
    struct timespec sleep_time = { 0, 100000000 };
    int cur_key_pressed;

    /* Process keypresses */
    while( 1 ) {
        squash_lock( display_info.lock );
        cur_key_pressed = getch();
        squash_unlock( display_info.lock );
        #ifdef INPUT_DEBUG
        squash_log( "Got key %d", cur_key_pressed );
        #endif
        switch( cur_key_pressed ) {
            case 12: /* CTRL-L (refresh) */
                do_clear();
            case KEY_RESIZE:
                do_resize();
                continue;
            case 'q':
            case 'Q':
                do_quit();
                break;
            case 'c':
            case 'C':
                do_toggle_spectrum_state();
                break;
            case 'p':
            case 'P':
                do_toggle_player_command();
                break;
            case 's':
            case 'S':
                do_set_player_command( CMD_SKIP );
                break;
            case 't':
            case 'T':
                do_set_player_command( CMD_STOP );
                break;
            case 'i':
            case 'I':
                do_toggle_info_command();
                break;
            case 'e':
            case 'E':
                do_edit_command();
                break;
            case 9:
                do_tab();
                break;
            case 'd':
            case 'D':
            case KEY_DC:
                do_delete();
                break;
            case KEY_UP:
                do_arrow(UP);
                break;
            case KEY_DOWN:
                do_arrow(DOWN);
                break;
            case KEY_PPAGE:
                do_arrow(PAGE_UP);
                break;
            case KEY_NPAGE:
                do_arrow(PAGE_DOWN);
                break;
            case KEY_HOME:
                do_arrow(HOME);
                break;
            case KEY_END:
                do_arrow(END);
                break;
            case KEY_LEFT:
                do_arrow(LEFT);
                break;
            case KEY_RIGHT:
                do_arrow(RIGHT);
                break;
            case ERR:
                /* getch() is not blocking, so if there is no key sleep */
                nanosleep( &sleep_time, NULL );
                break;
            default:
                break;
        }
    }

    return 0;
}

/*
 * The fifo thread function.  It monitors a fifo
 * for simple one line commands.
 * This allows for external programs to send commands to squash.
 */
void *fifo_monitor( void *input_data ) {
    char buffer[50]; /* can't be set higher than 512 for posix
                                            but needs to be bigger than the largest command */
    bool invalid_line;

    while( 1 ) {
        #ifdef INPUT_DEBUG
        squash_log("(re)Opening fifo");
        #endif
        if( (fifo_info.fifo_file = fopen(config.input_fifo_path, "r" )) == NULL ) {
            sleep(1);
            continue;
            /* ok so sometimes we can't open this... that's weird
            squash_error("Can't open squash command fifo for reading");
            */
        }

        /* Process FIFO data */
        invalid_line = FALSE;
        while( !feof(fifo_info.fifo_file) ) {
            buffer[0] = '\0';
            fgets( buffer, 50, fifo_info.fifo_file );
            if( buffer[strlen(buffer)-1] != '\n' ) {
                /* invalid line */
                invalid_line = TRUE;
                continue;
            }
            if( invalid_line == TRUE ) {
                invalid_line = FALSE;
                continue;
            }
            #ifdef INPUT_DEBUG
            squash_log("got: %s", buffer);
            #endif
            /* now buffer should have a command in it */
            if( strncasecmp( buffer, "refresh\n", 8 ) == 0 ) {
                do_clear();
                do_resize();
            } else if ( strncasecmp( buffer, "quit\n", 5 ) == 0 ) {
                do_quit();
            } else if ( strncasecmp( buffer, "spectrum_button\n", 16 ) == 0 ) {
                do_toggle_spectrum_state();
            } else if( strncasecmp( buffer, "play\n", 5 ) == 0 ) {
                do_set_player_command( CMD_PLAY );
            } else if ( strncasecmp( buffer, "pause\n", 6 ) == 0 ) {
                do_set_player_command( CMD_PAUSE );
            } else if ( strncasecmp( buffer, "play_button\n", 12 ) == 0 ) {
                do_toggle_player_command();
            } else if ( strncasecmp( buffer, "skip\n", 5 ) == 0 ) {
                do_set_player_command( CMD_SKIP );
            } else if ( strncasecmp( buffer, "stop\n", 5 ) == 0 ) {
                do_set_player_command( CMD_STOP );
            } else if ( strncasecmp( buffer, "info_button\n", 11 ) == 0 ) {
                do_toggle_info_command();
            } else if ( strncasecmp( buffer, "tab\n", 7 ) == 0 ) {
                do_tab();
            } else if ( strncasecmp( buffer, "edit\n", 5 ) == 0 ) {
                do_edit_command();
            } else if ( strncasecmp( buffer, "delete\n", 7 ) == 0 ) {
                do_delete();
            } else if ( strncasecmp( buffer, "up\n", 3 ) == 0 ) {
                do_arrow(UP);
            } else if ( strncasecmp( buffer, "down\n", 5 ) == 0 ) {
                do_arrow(DOWN);
            } else if ( strncasecmp( buffer, "pageup\n", 7 ) == 0 ) {
                do_arrow(PAGE_UP);
            } else if ( strncasecmp( buffer, "pagedown\n", 5 ) == 0 ) {
                do_arrow(PAGE_DOWN);
            } else if ( strncasecmp( buffer, "left\n", 5 ) == 0 ) {
                do_arrow(LEFT);
            } else if ( strncasecmp( buffer, "right\n", 6 ) == 0 ) {
                do_arrow(RIGHT);
            } else if ( strncasecmp( buffer, "home\n", 5 ) == 0 ) {
                do_arrow(HOME);
            } else if ( strncasecmp( buffer, "end\n", 4 ) == 0 ) {
                do_arrow(END);
            }
        }
        fclose( fifo_info.fifo_file );
    }

    return 0;
}

/*
 * Force a clear of the screen
 */
void do_clear( void ) {
    clear();
    refresh();
}

/*
 * Handle screen resize
 */
void do_resize( void ) {
    squash_lock( display_info.lock );
    draw_screen();
    squash_unlock( display_info.lock );
}

/*
 * Handle quiting the program
 */
void do_quit( void ) {
    /* Grab the status lock */
    squash_lock( status_info.lock );

    /* Setup status information */
    status_info.status_message = NULL;
    status_info.exit_status = 0;

    /* Release the status lock */
    squash_unlock( status_info.lock );

    /* Signal the main thread */
    squash_signal( status_info.exit );
}

/*
 * Set the play status
 */
void do_set_player_command( enum player_command_e command ) {
    /* Acquire Now Playing lock */
    squash_lock( player_command.lock );

    /* Set Command */
    player_queue_command( command );

    /* Release Now Playing Lock */
    squash_unlock( player_command.lock );

    /* Broadcast a new command is avaiable */
    squash_broadcast( player_command.changed );
}

/*
 * Toggle the play status
 */
void do_toggle_player_command( void ) {
    /* Acquire Now Playing lock */
    squash_lock( player_info.lock );
    squash_lock( player_command.lock );

    /* Determine which command to set */
    switch( player_info.state ) {
        case STATE_PLAY:
            player_queue_command( CMD_PAUSE );
            break;
        case STATE_PAUSE:
        case STATE_STOP:
            player_queue_command( CMD_PLAY );
            break;
        case STATE_BIG_STOP:
            /* ignore */
            break;
    }

    /* Release Now Playing Lock */
    squash_unlock( player_command.lock );
    squash_unlock( player_info.lock );

    /* Broadcast a new command is avaiable */
    squash_broadcast( player_command.changed );
}

/*
 * Set the spectrum window size.
 */
void do_set_spectrum_state( enum window_states_e new_state ) {
    /* Acquire required locks */
    squash_lock( display_info.lock );
    squash_lock( spectrum_ring.lock );

    display_info.window[ WIN_SPECTRUM ].state = new_state;

    switch( new_state ) {
        case WIN_STATE_NORMAL:
        case WIN_STATE_MAXIMIZED:
            spectrum_ring.active = TRUE;
            break;
        case WIN_STATE_HIDDEN:
            spectrum_ring.active = FALSE;
            break;
    }
    /* Release locks */
    squash_unlock( spectrum_ring.lock );
    draw_screen();
    squash_unlock( display_info.lock );

    squash_broadcast( spectrum_ring.changed );
}

/*
 * Toggle the spectrum window size.
 */
void do_toggle_spectrum_state( void ) {
    /* Acquire required locks */
    squash_lock( display_info.lock );
    squash_lock( spectrum_ring.lock );

    /* Change the spectrum state */
    switch( display_info.window[ WIN_SPECTRUM ].state ) {
        case WIN_STATE_NORMAL:
            display_info.window[ WIN_SPECTRUM ].state = WIN_STATE_MAXIMIZED;
            spectrum_ring.active = TRUE;
            break;
        case WIN_STATE_MAXIMIZED:
            display_info.window[ WIN_SPECTRUM ].state = WIN_STATE_HIDDEN;
            spectrum_ring.active = FALSE;
            break;
        case WIN_STATE_HIDDEN:
            display_info.window[ WIN_SPECTRUM ].state = WIN_STATE_NORMAL;
            spectrum_ring.active = TRUE;
            break;
    }
    /* Release locks */
    squash_unlock( spectrum_ring.lock );
    draw_screen();
    squash_unlock( display_info.lock );

    squash_broadcast( spectrum_ring.changed );
}

/*
 * Handle toggling info window
 */
void do_toggle_info_command( void ) {
    squash_lock( display_info.lock );
    switch( display_info.window[ WIN_INFO ].state ) {
        case WIN_STATE_NORMAL:
            display_info.window[ WIN_INFO ].state = WIN_STATE_HIDDEN;
            break;
        case WIN_STATE_HIDDEN:
        default:
            display_info.window[ WIN_INFO ].state = WIN_STATE_NORMAL;
            break;
    }
    draw_screen();
    squash_unlock( display_info.lock );
}

/*
 * Handle directional keys
 */
void do_arrow( enum direction_e direction ) {
    int delta;
    WINDOW *win;
    int xsize = 0, ysize = 0;

    squash_lock( display_info.lock );
    squash_rlock( database_info.lock );
    squash_lock( song_queue.lock );
    if( (win = display_info.window[WIN_PLAYLIST].window) != NULL ) {
            getmaxyx( win, ysize, xsize );
        ysize -= 3;
        if( ysize < 0) {
            ysize = 0;
        }
    }
    switch( direction ) {
        case UP:
            delta = -1;
            break;
        case DOWN:
            delta = +1;
            break;
        case PAGE_UP:
            delta = -ysize/2;
            break;
        case PAGE_DOWN:
            delta = +ysize/2;
            break;
        case HOME:
            delta = 0;
            song_queue.selected = 0;
            break;
        case END:
            song_queue.selected = 0;
            delta = song_queue.wanted_size > song_queue.size ? song_queue.wanted_size : song_queue.size;
            break;
        default:
            return;
    }
    if( song_queue.selected < 0) {
        song_queue.selected ^= -1;
    } else {
        song_queue.selected = song_queue.selected + delta;

        if( song_queue.selected < 0 ) {
            song_queue.selected = 0;
        } else {
            int size = song_queue.wanted_size > song_queue.size ? song_queue.wanted_size : song_queue.size;
            if( song_queue.selected >= size ) {
                song_queue.selected = size - 1;
            }
        }
    }

    squash_unlock( song_queue.lock );
    squash_runlock( database_info.lock );
    squash_unlock( display_info.lock );
    squash_broadcast( display_info.changed );
}

/*
 * Handle delete event
 */
void do_delete( void ) {
    int i;
    song_queue_entry_t *song_queue_entry, *song_queue_last_entry;

    squash_wlock( database_info.lock );
    squash_lock( song_queue.lock );
    if( song_queue.selected >= song_queue.size || song_queue.selected < 0 ) {
        squash_runlock( database_info.lock );
        squash_unlock( song_queue.lock );
        return;
    }
    song_queue_last_entry = NULL;
    song_queue_entry = song_queue.head;
    for( i = 0; i < song_queue.selected; i++ ) {
        song_queue_last_entry = song_queue_entry;
        song_queue_entry = song_queue_entry->next;
    }
    if( song_queue_last_entry != NULL ) {
        song_queue_last_entry->next = song_queue_entry->next;
    }
    if( song_queue_entry == song_queue.head ) {
        song_queue.head = song_queue_entry->next;
    }
    if( song_queue_entry == song_queue.tail ) {
        song_queue.tail = song_queue_last_entry;
    }
    feedback( song_queue_entry->song_info , -1);
    squash_free( song_queue_entry );
    song_queue.size--;
    squash_unlock( song_queue.lock );
    squash_wunlock( database_info.lock );
    squash_broadcast( song_queue.not_full );
    squash_broadcast( display_info.changed );
}

/*
 * Handle Tab event
 */
void do_tab( void ) {
    squash_lock( song_queue.lock );
    song_queue.selected ^= -1;
    squash_unlock( song_queue.lock );
    squash_broadcast( display_info.changed );
}

/*
 * Handle edit command
 */
void do_edit_command( void ) {
    char *editor;
    char *filename = NULL;
    int i;
    int pid;
    song_info_t *song;
    song_queue_entry_t *cur_entry;

    squash_rlock( database_info.lock );
    squash_lock( song_queue.lock );

    if( song_queue.selected >= song_queue.size ) {
        squash_unlock( song_queue.lock );
        squash_runlock( database_info.lock );

        return;
    } else if( song_queue.selected < 0 ) {
        squash_lock( player_info.lock );
        song = player_info.song;
        squash_unlock( player_info.lock );
        squash_unlock( song_queue.lock );
    } else {
        cur_entry = song_queue.head;
        for( i = 0; i < song_queue.selected; i++ ) {
            cur_entry = cur_entry->next;
        }

        song = cur_entry->song_info;

        squash_unlock( song_queue.lock );
    }
    if( song == NULL ) {
        squash_runlock( database_info.lock );
        return;
    }

    squash_asprintf(filename, "%s/%s.info", song->basename[ BASENAME_META ], song->filename);
    squash_runlock( database_info.lock );

    editor = getenv("EDITOR");
    if( editor == NULL ) {
        editor = "vi";
    }

    squash_lock( display_info.lock );
    endwin();
    reset_shell_mode();

    squash_log( "running '%s' '%s'", editor, filename );

    pid = fork();

    if( pid == 0 ) { /* the child */
        execlp( editor, editor, filename, 0 );
        /* be safe */
        return;
    }
    squash_free(filename);
    waitpid(pid, NULL, 0);

    squash_wlock( database_info.lock );
    clear_song_meta(song);
    load_meta_data(song, 0);
    load_meta_data(song, 1);
    squash_wunlock( database_info.lock );

    display_init();
    draw_screen();
    squash_unlock( display_info.lock );
}
