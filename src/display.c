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
 * display.c
 */

#include "global.h"
#include "spectrum.h"   /* for spectrum_resize() */
#include "database.h"   /* for get_meta_data() */
#include "stat.h"       /* for get_rating() */
#include "version.h"    /* for SQUASH_VERSION */
#include "display.h"

/*
 * Initialize the ncurses display and window system.
 */
void display_init( void ) {
    /* Initialize curses */
    (void) initscr();               /* Initialize the curses library */
    keypad( stdscr, TRUE );         /* Enable keyboard mapping */
    curs_set( 0 );                  /* Hide the cursor */
    (void) nonl();                  /* Tell curses not to do NL->CR/NL on output */
    (void) cbreak();                /* Take input chars one at a time, no wait for \n */
    (void) noecho();                /* Don't echo input */
    (void) nodelay( stdscr, TRUE ); /* Don't wait on getch() */

    /* Setup color */
    if( has_colors() ) {
        start_color();

        /* Define color pairs */
        init_pair( TEXT_BLACK, COLOR_BLACK, COLOR_BLACK );
        init_pair( TEXT_GREEN, COLOR_GREEN, COLOR_BLACK );
        init_pair( TEXT_RED, COLOR_RED, COLOR_BLACK );
        init_pair( TEXT_CYAN, COLOR_CYAN, COLOR_BLACK );
        init_pair( TEXT_WHITE, COLOR_WHITE, COLOR_BLACK );
        init_pair( TEXT_MAGENTA, COLOR_MAGENTA, COLOR_BLACK );
        init_pair( TEXT_BLUE, COLOR_BLUE, COLOR_BLACK );
        init_pair( TEXT_YELLOW, COLOR_YELLOW, COLOR_BLACK );
        init_pair( TEXT_BLUE_SELECTED, COLOR_WHITE, COLOR_BLUE );
        init_pair( TEXT_BLUE_BACKGROUND, COLOR_BLUE, COLOR_BLUE );
    }
}

/*
 * Set the windows to their initial values
 */
void window_init( void ) {
    display_info.window[ WIN_NOW_PLAYING ].is_fixed = 1;
    display_info.window[ WIN_NOW_PLAYING ].is_persistent = 1;
    display_info.window[ WIN_NOW_PLAYING ].size.fixed.height = 7;
    display_info.window[ WIN_NOW_PLAYING ].state = WIN_STATE_NORMAL;
    display_info.window[ WIN_NOW_PLAYING ].window = NULL;

    display_info.window[ WIN_INFO ].is_fixed = 1;
    display_info.window[ WIN_INFO ].is_persistent = 0;
    display_info.window[ WIN_INFO ].size.fixed.height = 9;
    display_info.window[ WIN_INFO ].state = WIN_STATE_NORMAL;
    display_info.window[ WIN_INFO ].window = NULL;

    display_info.window[ WIN_SPECTRUM ].is_fixed = 0;
    display_info.window[ WIN_SPECTRUM ].is_persistent = 0;
    display_info.window[ WIN_SPECTRUM ].size.calc.weight = 20;
    display_info.window[ WIN_SPECTRUM ].size.calc.min_height = 5;
    display_info.window[ WIN_SPECTRUM ].state = WIN_STATE_HIDDEN;
    display_info.window[ WIN_SPECTRUM ].window = NULL;

    display_info.window[ WIN_PLAYLIST ].is_fixed = 0;
    display_info.window[ WIN_PLAYLIST ].is_persistent = 0;
    display_info.window[ WIN_PLAYLIST ].size.calc.weight = 50;
    display_info.window[ WIN_PLAYLIST ].size.calc.min_height = 4;
    display_info.window[ WIN_PLAYLIST ].state = WIN_STATE_NORMAL;
    display_info.window[ WIN_PLAYLIST ].window = NULL;

    display_info.window[ WIN_HELP ].is_fixed = 1;
    display_info.window[ WIN_HELP ].is_persistent = 0;
    display_info.window[ WIN_HELP ].size.fixed.height = 3;
    display_info.window[ WIN_HELP ].state = WIN_STATE_NORMAL;
    display_info.window[ WIN_HELP ].window = NULL;
}

/*
 * Display thread function.  Updates the screen if any changes
 * are signaled by other threads.
 */
void *display_monitor( void *input_data ) {
    /* Loop forever waiting for changes to be signaled */
    while( 1 ) {
        /* Acquire display lock */
        squash_lock( display_info.lock );

        /* Wait for an update */
        squash_wait( display_info.changed, display_info.lock );

        /* Grab additional locks */
        squash_rlock( database_info.lock );
        squash_lock( song_queue.lock );
        squash_lock( player_info.lock );

        /* Update the screen */
        draw_now_playing();
        draw_playlist();
        draw_info();
        draw_help();

        /* Release locks */
        squash_unlock( player_info.lock );
        squash_unlock( song_queue.lock );
        squash_runlock( database_info.lock );
        squash_unlock( display_info.lock );
    }

    /* Shouldn't ever get here */
    return (void *)NULL;
}

/*
 * Draw the screen,  This takes into account rebuilding all the
 * windows in case of screen resize, or if a window changes size.
 */
void draw_screen( void ) {
    int screen_width, screen_height;
    int available_rows, used_rows;
    int calc_window_weight_total;
    int cur_top;
    int maximized_window;

    int new_spectrum_height = 0, new_spectrum_width = 0;
    int i;

    /* Determine the screen dimensions */
    getmaxyx( stdscr, screen_height, screen_width );

    /* Find out if there is a maximized window */
    maximized_window = -1;
    for( i = 0; i < WIN_COUNT; i++ ) {
        if( display_info.window[i].state == WIN_STATE_MAXIMIZED ) {
            if( display_info.window[i].is_fixed || maximized_window >= 0 ) {
                display_info.window[i].state = WIN_STATE_NORMAL;
            } else {
                maximized_window = i;
            }
        }
    }

    /* Calculate screen information */
    available_rows = screen_height;
    used_rows = 0;
    calc_window_weight_total = 0;
    for( i = 0; i < WIN_COUNT; i++ ) {
        /* Delete any existing window */
        if( display_info.window[i].window != NULL ) {
            werase( display_info.window[i].window );
            delwin( display_info.window[i].window );
            display_info.window[i].window = NULL;
        }

        /* Make sure we are going to display this window */
        if( display_info.window[i].state == WIN_STATE_HIDDEN ) {
            continue;
        }

        /* Calculate available and required rows */
        if( maximized_window >= 0 ) {
            if( i != maximized_window && display_info.window[i].is_persistent ) {
                if( !display_info.window[i].is_fixed ) {
                    display_info.window[i].size.calc.height = display_info.window[i].size.calc.min_height;
                }
                available_rows -= display_info.window[i].size.calc.height;
            } else if( i == maximized_window ) {
                available_rows -= display_info.window[i].size.calc.min_height;
            }
        } else {
            if( !display_info.window[i].is_fixed ) {
                calc_window_weight_total += display_info.window[i].size.calc.weight;
                available_rows -= display_info.window[i].size.calc.min_height;
            } else {
                available_rows -= display_info.window[i].size.fixed.height;
            }
        }
    }

    squash_log( "Screen size: %d x %d", screen_width, screen_height );
    squash_log( "Available Rows: %d", available_rows );

    /* Make sure there is enough screen space */
    if( (available_rows < 0) || (screen_width < 45) ) {
        /* Display an error on stdscr */
        mvprintw( 0, 0, "Screen too small" );
        refresh();

        return;
    }

    /* Calculate window heights */
    if( maximized_window >= 0 ) {
        display_info.window[ maximized_window ].size.calc.height = available_rows + display_info.window[ maximized_window ].size.calc.min_height;
    } else {
        for( i = 0; i < WIN_COUNT; i++ ) {
            /* Make sure this window should be displayed */
            if( display_info.window[i].state == WIN_STATE_HIDDEN ) {
                continue;
            }

            /* Set auto-sized heights */
            if( !display_info.window[i].is_fixed ) {
                /* Calculate the desired size (rounding up) */
                display_info.window[i].size.calc.height = ceil( (double)available_rows * display_info.window[i].size.calc.weight / calc_window_weight_total );

                /* Clip the height based on available space */
                if( (available_rows - used_rows) < display_info.window[i].size.calc.height ) {
                    display_info.window[i].size.calc.height = available_rows - used_rows;
                }

                /* Increment the number of rows used */
                used_rows += display_info.window[i].size.calc.height;

                /* Add in the min. height for the window */
                display_info.window[i].size.calc.height += display_info.window[i].size.calc.min_height;
            }
        }
    }

    /* Create Windows */
    cur_top = 0;
    for( i = 0; i < WIN_COUNT; i++ ) {
        /* Make sure this window should be displayed */
        if( (display_info.window[i].state == WIN_STATE_HIDDEN) || ( (maximized_window != i)  && (maximized_window >= 0) && (!display_info.window[i].is_persistent)) ) {
            continue;
        }

        /* Create new window */
        display_info.window[ i ].window = newwin( display_info.window[i].size.calc.height, screen_width, cur_top, 0 );

        /* Update the top */
        cur_top += display_info.window[i].size.calc.height;
    }

    /* Acquire locks */
    squash_rlock( database_info.lock );
    squash_lock( song_queue.lock );
    squash_lock( player_info.lock );
    squash_lock( spectrum_info.lock );

    /* Get the spectrum window's dimensions */
    if( display_info.window[WIN_SPECTRUM].window == NULL ) {
        new_spectrum_height = 0;
        new_spectrum_width = 0;
    } else {
        getmaxyx( display_info.window[WIN_SPECTRUM].window, new_spectrum_height, new_spectrum_width );
    }

    /* Update the spectrum window sizes */
    spectrum_resize(new_spectrum_height, new_spectrum_width);

    /* Draw Sections */
    draw_spectrum();
    draw_help();
    draw_now_playing();
    draw_playlist();
    draw_info();

    /* Release locks */
    squash_unlock( spectrum_info.lock );
    squash_unlock( player_info.lock );
    squash_unlock( song_queue.lock );
    squash_runlock( database_info.lock );
}

/*
 * Draw the now playing window
 */
void draw_now_playing( void ) {
    WINDOW *win;
    int win_height, win_width;
    int text_color;
    song_info_t *cur_song;
    long cur_duration, cur_position;
    int time_left;

    /* Set the window */
    if( (win = display_info.window[WIN_NOW_PLAYING].window) == NULL ) {
        return;
    }

    /* Get window size */
    getmaxyx( win, win_height, win_width );

    /* Clear the window */
    werase( win );

    /* Set the text color */
    text_color = COLOR_PAIR( TEXT_RED );

    /* Setup color */
    wattron( win, text_color);

    /* Draw the border */
    wborder( win, 0, 0, 0, 0, 0, 0, 0, 0 );

    wmove( win, win_height - 1, 0 );
    whline( win, ' ', win_width );

    mvwaddch( win, win_height - 2, 0, ACS_LLCORNER );
    whline( win, 0, win_width - 2 );
    mvwaddch( win, win_height - 2, win_width - 1, ACS_LRCORNER );

    /* Print the header */
    mvwprintw( win, 0, 2, "Now Playing" );

    /* Reset color */
    wattroff( win, text_color );

    /* Get Song */
    cur_song = player_info.song;
    cur_duration = player_info.duration / 1000;
    cur_position = player_info.current_position / 1000;

    /* Make sure we have a song to display */
    if( cur_song == NULL ) {
        wattron( win, COLOR_PAIR(TEXT_RED) );
        if( display_info.state == SYSTEM_LOADING ) {
            mvwprintw( win, 2, (win_width / 2) - 12, "-- Database loading.. --" );
        } else {
            mvwprintw( win, 2, (win_width / 2) - 12, "-- No Songs Available --" );
        }
        wattroff( win, COLOR_PAIR(TEXT_RED) );
        wrefresh( win );
        return;
    }

    /* Print information */
    mvwprintw( win, 1, 2, "Title:  " );
    mvwprintw( win, 2, 2, "Artist: " );
    mvwprintw( win, 3, 2, "Album:  " );
    mvwprintw( win, 4, 2, "Track:  " );

    /* Calculate the time position */
    time_left = win_width - num_chars(cur_position / 60) - num_chars(cur_duration / 60) - 8;

    /* Print song information */
    draw_meta_string( win, cur_song, "title", 1, 10, win_width - 16 );
    draw_meta_string( win, cur_song, "artist", 2, 10, win_width - 16 );
    draw_meta_string( win, cur_song, "album", 3, 10, time_left - 12 );
    draw_meta_string( win, cur_song, "tracknumber", 4, 10, 9 );

    /* Process Time Information */
    if( cur_duration >= 0 ) {
        int bar_left, bar_width, bar_filled;

        /* Calculate progress bar */
        bar_width = (win_width - 20) * .7;
        bar_left = win_width - bar_width - 2;
        bar_filled = (int)( ((double)cur_position / cur_duration * bar_width) + 0.5 );

        /* Print progress bar box */
        wattron( win, text_color );

        mvwaddch( win, 4, bar_left, ACS_ULCORNER );
        whline( win, ACS_HLINE, bar_width );
        mvwaddch( win, 4, win_width - 1, ACS_RTEE );

        mvwaddch( win, 5, bar_left, ACS_RTEE );
        whline( win, ' ', bar_width );
        wattron( win, A_BOLD );
        whline( win, ACS_CKBOARD, bar_filled );
        wattroff( win, A_BOLD );
        mvwaddch( win, 5, win_width - 1, ACS_VLINE );

        mvwaddch( win, 6, bar_left, ACS_LLCORNER );
        whline( win, ACS_HLINE, bar_width );
        mvwaddch( win, 6, win_width - 1, ACS_LRCORNER );

        wattroff( win, text_color );

        /* Print Time Information */
        mvwprintw(    win, 3, time_left, "%d:%02d/%d:%02d",
            (int)(cur_position / 60),
            (int)(cur_position % 60),
            (int)(cur_duration / 60),
            (int)(cur_duration % 60) );
    }

    /* Refresh the window */
    wrefresh( win );
}

/*
 * Draw the playlist window
 */
void draw_playlist( void ) {
    WINDOW *win;
    int win_height, win_width;
    int col_left[ 3 ];
    int col_width;
    int list_height, start_index, max_size;
    int text_color, select_color;
    int i;

    /* Set the window */
    if( (win = display_info.window[WIN_PLAYLIST].window) == NULL ) {
        return;
    }

    /* Get window size */
    getmaxyx( win, win_height, win_width );

    /* Clear the screen */
    werase( win );

    /* Set the text color */
    text_color = COLOR_PAIR( TEXT_BLUE ) | A_BOLD;

    /* Setup the window color */
    wattron( win, text_color );

    /* Draw the border */
    wborder( win, 0, 0, 0, 0, 0, 0, 0, 0 );

    /* Calculate column positions */
    col_width = ( win_width - 8 ) / 3;
    col_left[ 0 ] = 2;
    col_left[ 1 ] = col_width + 4;
    col_left[ 2 ] = (2 * col_width) + 6;

    /* Add the column header line */
    mvwaddch( win, 1, 0, ACS_LTEE );
    whline( win, 0, win_width - 2 );
    mvwaddch( win, 1, win_width - 1, ACS_RTEE );

    /* Print Headers */
    mvwprintw( win, 0, 2, "Up Next" );
    mvwaddstr( win, 1, col_left[0], "Artist" );
    mvwaddstr( win, 1, col_left[1], "Title" );
    mvwaddstr( win, 1, col_left[2], "Album" );

    /* Reset item color */
    wattroff( win, text_color );

    /* Setup row information */
    list_height = win_height - 3;

    /* Process Playlist */
    if( song_queue.size <= 0 ) {
        wattron( win, COLOR_PAIR(TEXT_RED) );
        if( display_info.state == SYSTEM_LOADING ) {
            mvwprintw( win, 3, (win_width / 2) - 14, "-- Database loading...... --" );
        } else {
            mvwprintw( win, 3, (win_width / 2) - 14, "-- No Entries in Playlist --" );
        }
        wattroff( win, COLOR_PAIR(TEXT_RED) );
    } else {
        song_queue_entry_t *cur_song_queue_entry;

        /* If our size is less than it will eventually be */
        if( song_queue.size < song_queue.wanted_size ) {
            mvwprintw( win, 0, 10, "(loading)");
        /* otherwise, we are as big as we are going to get, so
           if our selection is too big clip it */
        } else if ( song_queue.selected >= song_queue.size ) {
            song_queue.selected = song_queue.size - 1;
        }

        /* Update the number of rows to display */
        if( song_queue.size <= list_height ) {
            list_height = song_queue.size;
            start_index = 0;
        } else {
            start_index = song_queue.selected - list_height / 2;
            max_size = song_queue.size > song_queue.wanted_size ? song_queue.size : song_queue.wanted_size;

            /* Check boundries */
            if( start_index < 0 ) {
                start_index = 0;
            } else if( (start_index + list_height) > max_size ) {
                start_index = max_size - list_height;
            }

            /* Print Scroll Arrows */
            if( start_index > 0 ) {
                mvwaddch( win, 2, win_width - 2, ACS_UARROW );
            }
            if( (start_index + list_height) < song_queue.size ) {
                mvwaddch( win, win_height - 2, win_width - 2, ACS_DARROW );
            }
        }

        /*
         * Display playlist
         */
        cur_song_queue_entry = song_queue.head;
        select_color = COLOR_PAIR( TEXT_BLUE_SELECTED ) | A_BOLD;

        /* Draw Selected Line */
        if( song_queue.selected >= 0 ) {
            wattron( win, COLOR_PAIR(TEXT_BLUE_BACKGROUND) );
            wmove( win, song_queue.selected - start_index + 2, 1 );
            whline( win, 0, win_width - 2 );
            wattroff( win, COLOR_PAIR(TEXT_BLUE_BACKGROUND) );
        }

        /* Move to the correct queue entry */
        for( i = 0; i < start_index && cur_song_queue_entry != NULL; i++ ) {
            cur_song_queue_entry = cur_song_queue_entry->next;
        }

        /* Print the queue */
        for( i = 0; i < list_height; i++ ) {
            /* Allow for incorrect size information */
            if( cur_song_queue_entry == NULL ) {
                break;
            }

            if( (i + start_index) == song_queue.selected ) {
                wattron( win, select_color );
            }

            /* Print playlist information */
            draw_meta_string( win, cur_song_queue_entry->song_info, "artist", i + 2, col_left[0], col_width );
            draw_meta_string( win, cur_song_queue_entry->song_info, "title", i + 2, col_left[1], col_width );
            draw_meta_string( win, cur_song_queue_entry->song_info, "album", i + 2, col_left[2], col_width );

            if( (i + start_index) == song_queue.selected ) {
                wattroff( win, select_color );
            }

            /* Load the next entry */
            cur_song_queue_entry = cur_song_queue_entry->next;
        }
    }

    /* Refresh Changes */
    wrefresh( win );
}

/*
 * Draw the spectrum window.
 */
void draw_spectrum( void ) {
    WINDOW *win;
    int win_height, win_width;
    int text_color;

    /* Set the window */
    if( (win = display_info.window[WIN_SPECTRUM].window) == NULL ) {
        return;
    }

    /* Get window size */
    getmaxyx( win, win_height, win_width );

    /* Clear the window */
    werase( win );

    /* Set the text color */
    text_color = COLOR_PAIR( TEXT_GREEN );

    /* Setup color */
    wattron( win, text_color);

    /* Draw the border */
    wborder( win, 0, 0, 0, 0, 0, 0, 0, 0 );

    /* Print the header */
    mvwprintw( win, 0, 2, "Spectrum Analyzer" );

    /* Only print something inside the spectrum windo if we have a bitmap,
       and it was rendered using the same height we are currently. */
    if( (spectrum_info.bitmap != NULL) && (spectrum_info.height == win_height - 2) && (spectrum_info.width == win_width - 2) ) {
        int row_top[ 3 ];
        int x, y;
        int cur_color = 0;

        /* Setup row positions */
        row_top[ 0 ] = (win_height - 2) * 0 / 3;
        row_top[ 1 ] = (win_height - 2) * 1 / 3;
        row_top[ 2 ] = (win_height - 2) * 2 / 3;

        for( y = 0; y < win_height - 2; y++ ) {
            if( y == row_top[cur_color] ) {
                /* Reset Previous Color */
                wattroff( win, text_color );

                /* Determine new color */
                switch( cur_color++ ) {
                    case 0:
                        text_color = COLOR_PAIR(TEXT_GREEN) | A_BOLD;
                        break;
                    case 1:
                        text_color = COLOR_PAIR(TEXT_YELLOW) | A_BOLD;
                        break;
                    case 2:
                        text_color = COLOR_PAIR(TEXT_RED) | A_BOLD;
                        break;
                    default:
                        text_color = COLOR_PAIR(TEXT_BLUE) | A_BOLD;
                        break;
                }

                /* Set new color */
                wattron( win, text_color );
            }

            /* Print string */
            for( x = 0; x < win_width - 2; x++ ) {
                mvwaddch( win, win_height - y - 2, x + 1, spectrum_info.bitmap[(long)y * (win_width - 2) + (long)x] );
            }
        }
    }

    /* Reset last color */
    wattroff( win, text_color );

    /* Refresh Changes */
    wrefresh( win );
}

/*
 * Draw the help window
 */
void draw_help( void ) {
    WINDOW *win;
    int win_height, win_width;
    char *version = "Squash version " SQUASH_VERSION " by Adam Luter";

    /* Set the window */
    if( (win = display_info.window[WIN_HELP].window) == NULL ) {
        return;
    }

    /* Get window size */
    getmaxyx( win, win_height, win_width );

    mvwprintw( win, 2, win_width - strlen(version) - 1, version );

    /* Print Text */
    mvwaddstr( win, 0 , 2, "Quit" );
    mvwaddstr( win, 1 , 2, "Spectrum" );
    mvwaddstr( win, 0 , 14, "Play/Pause" );
    mvwaddstr( win, 1 , 14, "Stop" );
    mvwaddstr( win, 0 , 26, "Skip" );
    mvwaddstr( win, 1 , 26, "Edit" );
    mvwaddstr( win, 0 , 38, "Info" );

    /* Print Keys */
    wattron( win, A_BOLD );
    mvwaddch( win, 0, 2, 'Q' );
    mvwaddch( win, 1, 5, 'c' );
    mvwaddch( win, 0, 14, 'P' );
    mvwaddch( win, 1, 15, 't' );
    mvwaddch( win, 0, 26, 'S' );
    mvwaddch( win, 1, 26, 'E' );
    mvwaddch( win, 0, 38, 'I' );
    wattroff( win, A_BOLD );

    /* Refresh Changes */
    wrefresh( win );
}

/*
 * Draw the info window.
 */
void draw_info( void ) {
    WINDOW *win;
    int win_height, win_width;
    double avg, std_dev;
    int i;
    double rating;
    int play_count, skip_count;
    song_queue_entry_t *queue_entry;
    song_info_t *song;
    char *filename;

    /* Set the window */
    if( (win = display_info.window[WIN_INFO].window) == NULL ) {
        return;
    }

    /* Clear conents */
    werase( win );

    /* Get window size */
    getmaxyx( win, win_height, win_width );

    /* Draw the border */
    wborder( win, 0, 0, 0, 0, 0, 0, 0, 0 );

    /* Print the header */
    mvwprintw( win, 0, 2, "Status" );

    /* Determine which song to print */
    if( song_queue.selected < 0 ) {
        /* selected is the now playing window */
        song = player_info.song;
    } else {
        /* currently not selecting anything (delete at the end of the list) */
        if( song_queue.size - 1 < song_queue.selected ) {
            song = NULL;
        } else {
            /* Otherwise go and find the entry on the queue */
            queue_entry = song_queue.head;
            for( i = 0; i < song_queue.selected; i++ ) {
                queue_entry = queue_entry->next;
                if( queue_entry == NULL ) {
                    /* shouldn't happen */
                    break;
                }
            }
            if( queue_entry == NULL ) {
                song = NULL;
            } else {
                song = queue_entry->song_info;
            }
        }
    }

    /* Set the selected song's statistics */
    if( song == NULL ) {
        filename = "No song selected";
        rating = 0.0;
        play_count = 0;
        skip_count = 0;
    } else {
        filename = song->filename;
        rating = get_rating( song->stat );
        play_count = song->stat.play_count;
        skip_count = song->stat.skip_count;
    }

    /* Clip filename */
    filename = strndup( filename, win_width - 2 );

    /* Display filename and songs loaded */
    mvwprintw( win, 1, 1, "Current Selected Song filename:" );
    mvwprintw( win, 2, 1, "%s", filename );
    mvwprintw( win, 3, 1, "Songs loaded: %d", database_info.song_count );

    /* Free filename */
    squash_free( filename );

    /* If there are songs, and we aren't still loading the database print out
       the various statistics */
    if( database_info.song_count != 0 && display_info.state != SYSTEM_LOADING ) {
        mvwprintw( win, 4, 1, "              Current /  Average /  Std Dev" );
        avg = database_info.sum / database_info.song_count;
        std_dev = sqrt( fabs(database_info.sqr_sum / database_info.song_count - avg*avg) );
        mvwprintw( win, 5, 1, "Rating:      % 8.5f / % 8.5f / % 8.5f", rating, avg, std_dev );
        avg = (double)database_info.play_sum / database_info.song_count;
        std_dev = sqrt( fabs((double)database_info.play_sqr_sum / database_info.song_count - avg*avg) );
        mvwprintw( win, 6, 1, "Play Count:  % 8d"" / % 8.5f / % 8.5f", play_count, avg, std_dev );
        avg = (double)database_info.skip_sum / database_info.song_count;
        std_dev = sqrt( fabs((double)database_info.skip_sqr_sum / database_info.song_count - avg*avg) );
        mvwprintw( win, 7, 1, "Skip Count:  % 8d"" / % 8.5f / % 8.5f", skip_count, avg, std_dev );
    }

    /* Refresh Changes */
    wrefresh( win );
}

/*
 * Draws all values for a key within a song entry.
 * Will also take into account limited display space.
 */
void draw_meta_string( WINDOW *win, song_info_t *song, char *meta_key, int top, int left, int width ) {
    char *line_buffer;
    char *cur_format;
    meta_key_t *meta_data;
    int line_width;
    int i;

    /* Allocate space for the buffer */
    squash_malloc( line_buffer, width + 1 );

    /* Build the string */
    line_width = 0;
    line_buffer[0] = '\0';
    cur_format = "%s; ";

    meta_data = get_meta_data( song, meta_key );

    if( meta_data == NULL || meta_data->value_count == 0 ) {
        line_width += snprintf( line_buffer, width + 1, "[Unknown]" );
    } else {
        for( i = 0; (i < meta_data->value_count) && (line_width < width); i++ ) {
            /* Change the format if this is the last item */
            if( i == (meta_data->value_count - 1) ) {
                cur_format = "%s";
            }

            /* Add the current value to the line */
            line_width += snprintf( line_buffer + line_width, width - line_width + 1, cur_format, meta_data->values[i] );
        }
    }

    /* Show elipse if required */
    if( line_width > width ) {
        line_buffer[ width - 1 ] = '.';
        line_buffer[ width - 2 ] = '.';
        line_buffer[ width - 3 ] = '.';
    }

    /* Print the line */
    mvwprintw( win, top, left, line_buffer );

    /* Cleanup */
    squash_free( line_buffer );
}

/*
 * Returns the number of characters a whole number will take up.
 */
int num_chars( long value ) {
    long num_chars;

    num_chars = (int)log10( (double)value ) + 1;

    if( num_chars < 1 ) {
        return 1;
    }

    return num_chars;
}
