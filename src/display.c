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
#ifdef EMPEG
    #include "vfdlib.h" /* for vfdlib_*() */
#endif
#include "display.h"

void set_display_brightness_empeg(int brightness) {
#ifdef EMPEG
    if( brightness < 0 ) {
        brightness = 0;
    } else if (brightness > 100 ) {
        brightness = 100;
    }
    ioctl(display_info.screen_fd, _IOW('d', 11, int), &brightness);
#endif
}

/*
 * Initialize the ncurses display and window system.
 */
void display_init( void ) {
#ifdef EMPEG
    if( (display_info.screen_fd = open( "/dev/display", O_RDWR )) == -1 ) {
        squash_error("Can't open /dev/display");
    }
    if( (int)(display_info.screen = mmap( 0, 2048, PROT_READ | PROT_WRITE, MAP_SHARED, display_info.screen_fd, 0 )) == -1 ) {
        squash_error("Can't mmap /dev/display");
    }
    ioctl(display_info.screen_fd, _IOW('d', 1, int), 1);
    vfdlib_clear( display_info.screen, 0 );
    vfdlib_drawText( display_info.screen, "Squash Loading, version " SQUASH_VERSION, 0, 0, 2, 3);
    ioctl(display_info.screen_fd, _IO('d', 0));

    display_info.brightness = 100;
    set_display_brightness_empeg( display_info.brightness );
#endif
#ifndef NO_NCURSES
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

    display_info.too_small = FALSE;
#endif
}

#ifndef NO_NCURSES
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

    display_info.window[ WIN_PASTLIST ].is_fixed = 0;
    display_info.window[ WIN_PASTLIST ].is_persistent = 0;
    display_info.window[ WIN_PASTLIST ].size.calc.weight = 50;
    display_info.window[ WIN_PASTLIST ].size.calc.min_height = 4;
    display_info.window[ WIN_PASTLIST ].state = WIN_STATE_HIDDEN;
    display_info.window[ WIN_PASTLIST ].window = NULL;

    display_info.window[ WIN_HELP ].is_fixed = 1;
    display_info.window[ WIN_HELP ].is_persistent = 0;
    display_info.window[ WIN_HELP ].size.fixed.height = 3;
    display_info.window[ WIN_HELP ].state = WIN_STATE_NORMAL;
    display_info.window[ WIN_HELP ].window = NULL;
}
#endif

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
        squash_lock( past_queue.lock );
        squash_lock( player_info.lock );

        /* Update the screen */
#ifdef EMPEG
        draw_empeg_display();
#endif
#ifndef NO_NCURSES
        draw_now_playing();
        draw_list( WIN_PLAYLIST );
        draw_list( WIN_PASTLIST );
        draw_info();
        draw_help();
#endif

        /* Release locks */
        squash_unlock( player_info.lock );
        squash_unlock( song_queue.lock );
        squash_unlock( past_queue.lock );
        squash_runlock( database_info.lock );
        squash_unlock( display_info.lock );
    }

    return (void *)NULL;
}

/*
 * Draw the screen,  This takes into account rebuilding all the
 * windows in case of screen resize, or if a window changes size.
 */
void draw_screen( void ) {
#ifndef NO_NCURSES
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
        clear();
        mvprintw( 0, 0, "Screen too small" );
        refresh();
        display_info.too_small = TRUE;

        return;
    } else {
        display_info.too_small = FALSE;
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
    squash_lock( past_queue.lock );
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
    draw_list( WIN_PLAYLIST );
    draw_list( WIN_PASTLIST );
    draw_info();

    /* Release locks */
    squash_unlock( spectrum_info.lock );
    squash_unlock( player_info.lock );
    squash_unlock( past_queue.lock );
    squash_unlock( song_queue.lock );
    squash_runlock( database_info.lock );
#endif
}

#ifdef EMPEG
#define WIDTH 128

void draw_stars_empeg( song_info_t *song ) {
    unsigned char whole_star[] = { BITMAP_2BPP, 0, 9, 0, 9,
        0x00, 0x03, 0x00, /*0000 0000 1100 0000 0000 0000*/
        0x00, 0x03, 0x00, /*0000 0000 1100 0000 0000 0000*/
        0xff, 0xff, 0xff, /*1111 1111 1111 1111 1100 0000*/
        0xf0, 0x3f, 0x00, /*0000 1111 1111 1100 0000 0000*/
        0xc0, 0x0f, 0x00, /*0000 0011 1111 0000 0000 0000*/
        0xf0, 0x3f, 0x00, /*0000 1111 1111 1100 0000 0000*/
        0xf0, 0x3c, 0x00, /*0000 1111 0011 1100 0000 0000*/
        0x3c, 0xf0, 0x00, /*0011 1100 0000 1111 0000 0000*/
        0x0c, 0xc0, 0x00  /*0011 0000 0000 0011 0000 0000*/
    };
    unsigned char half_star[] = { BITMAP_2BPP, 0, 9, 0, 9,
        0x00, 0x03, 0x00, /*0000 0000 1100 0000 0000 0000*/
        0x00, 0x03, 0x00, /*0000 0000 1100 0000 0000 0000*/
        0xff, 0xab, 0x02, /*1111 1111 1101 0101 0100 0000*/
        0xf0, 0x2b, 0x00, /*0000 1111 1101 0100 0000 0000*/
        0xc0, 0x0b, 0x00, /*0000 0011 1101 0000 0000 0000*/
        0xf0, 0x2b, 0x00, /*0000 1111 1101 0100 0000 0000*/
        0xf0, 0x28, 0x00, /*0000 1111 0001 0100 0000 0000*/
        0x3c, 0xa0, 0x00, /*0011 1100 0000 0101 0000 0000*/
        0x0c, 0x80, 0x00  /*0011 0000 0000 0001 0000 0000*/
    };
    unsigned char no_star[] = { BITMAP_2BPP, 0, 9, 0, 9,
        0x00, 0x02, 0x00, /*0000 0000 0100 0000 0000 0000*/
        0x00, 0x02, 0x00, /*0000 0000 0100 0000 0000 0000*/
        0xaa, 0xaa, 0x02, /*0101 0101 0101 0101 0100 0000*/
        0xa0, 0x2a, 0x00, /*0000 0101 0101 0100 0000 0000*/
        0x80, 0x0a, 0x00, /*0000 0001 0101 0000 0000 0000*/
        0xa0, 0x2a, 0x00, /*0000 0101 0101 0100 0000 0000*/
        0xa0, 0x28, 0x00, /*0000 0101 0001 0100 0000 0000*/
        0x28, 0xa0, 0x00, /*0001 0100 0000 0101 0000 0000*/
        0x08, 0x80, 0x00  /*0001 0000 0000 0001 0000 0000*/
    };
    int rating, i;

    if( !song ) {
        return;
    }

    draw_string_empeg( display_info.screen, "Please Rate this song:", 6, 0, WIDTH );

    {
        char *buf;
        asprintf(&buf, "(Current rating %+6.3f)", get_rating( song->stat ) );
        draw_string_empeg( display_info.screen, buf, 12, 0, WIDTH );
        squash_free( buf );
    }

    rating = song->stat.manual_rating;

    if( rating == -1 ) {
        vfdlib_drawText( display_info.screen, "?", 12, 22, 2, 3);
    } else {
        vfdlib_drawText( display_info.screen, "?", 12, 22, 2, 2);
    }

    for( i = 0; i < rating / 2; i++ ) {
        vfdlib_drawBitmap( display_info.screen, whole_star,
            20 + i*12, 20, /* dest x,y */
            0, 0,
            -1, -1,
            -1, FALSE);
    }
    if( rating % 2 == 1 ) {
        vfdlib_drawBitmap( display_info.screen, half_star,
            20 + i*12, 20, /* dest x,y */
            0, 0,
            -1, -1,
            -1, FALSE);
        i++;
    }
    for( ; i < 5; i++ ) {
        vfdlib_drawBitmap( display_info.screen, no_star,
            20 + i*12, 20, /* dest x,y */
            0, 0,
            -1, -1,
            -1, FALSE);
    }

}

void draw_song_empeg( song_info_t *song, bool current_song ) {
    key_set_t title_set = (key_set_t) { (char *[]){"title"}, 1 };
    key_set_t artist_set = (key_set_t) { (char *[]){"artist", "artist_original"}, 2 };
    key_set_t album_set = (key_set_t) { (char *[]){"album", "album_original", "source"}, 3 };
    key_set_t tracknumber_set = (key_set_t) { (char *[]){"tracknumber", "tracknr"}, 2 };
    unsigned char small_whole_star[] = { BITMAP_2BPP, 0, 5, 0, 5,
        0x30, 0x00, /* 0000 1100 0000 0000 */
        0xff, 0x03, /* 1111 1111 1100 0000 */
        0xfc, 0x00, /* 0011 1111 0000 0000 */
        0xcf, 0x03, /* 1111 0011 1100 0000 */
        0x03, 0x03  /* 1100 0000 1100 0000 */
    };
    unsigned char small_half_star[] = { BITMAP_2BPP, 0, 5, 0, 5,
        0x30, 0x00, /* 0000 1100 0000 0000 */
        0xbf, 0x02, /* 1111 1101 0100 0000 */
        0xbc, 0x00, /* 0011 1101 0000 0000 */
        0x8f, 0x02, /* 1111 0001 0100 0000 */
        0x03, 0x02  /* 1100 0000 0100 0000 */
    };
    unsigned char small_no_star[] = { BITMAP_2BPP, 0, 5, 0, 5,
        0x20, 0x00, /* 0000 0100 0000 0000 */
        0xaa, 0x02, /* 0101 0101 0100 0000 */
        0xa8, 0x00, /* 0001 0101 0000 0000 */
        0x8a, 0x02, /* 0101 0001 0100 0000 */
        0x02, 0x02  /* 0100 0000 0100 0000 */
    };
    int rating, i;

    if( current_song && song ) {
        rating = song->stat.manual_rating;

        if( rating == -1 ) {
            vfdlib_drawText( display_info.screen, "?", 10, 0, 2, 3);
        } else {
            vfdlib_drawText( display_info.screen, "?", 10, 0, 2, 2);
        }

        for( i = 0; i < rating / 2; i++ ) {
            vfdlib_drawBitmap( display_info.screen, small_whole_star,
                14 + i*6, 0, /* dest x,y */
                0, 0,
                -1, -1,
                -1, FALSE);
        }
        if( rating % 2 == 1 ) {
            vfdlib_drawBitmap( display_info.screen, small_half_star,
                14 + i*6, 0, /* dest x,y */
                0, 0,
                -1, -1,
                -1, FALSE);
            i++;
        }
        for( ; i < 5; i++ ) {
            vfdlib_drawBitmap( display_info.screen, small_no_star,
                14 + i*6, 0, /* dest x,y */
                0, 0,
                -1, -1,
                -1, FALSE);
        }

        {
            char buf[7];
            snprintf(buf, 7, "%+6.3f", get_rating( song->stat ) );
            draw_string_monospaced_empeg( display_info.screen, buf, 0, 44, 4 );
        }
    }

    vfdlib_drawLineVertClipped( display_info.screen, WIDTH, 0, 31, 2);
    vfdlib_drawLineHorizClipped( display_info.screen, 24, 0, WIDTH, 2);
    draw_meta_string_empeg( display_info.screen, song, title_set, 6, 0, WIDTH );
    draw_meta_string_empeg( display_info.screen, song, artist_set, 12, 0, WIDTH );
    draw_meta_string_empeg( display_info.screen, song, album_set, 18, 0, WIDTH );
    draw_string_empeg( display_info.screen, "Track: ", 26, 4*11+3, WIDTH-4*11-3 );
    draw_meta_string_empeg( display_info.screen, song, tracknumber_set, 26, 4*11+3+6*4, WIDTH-4*11-3-6*4 );
    if( song ) {
        if( current_song ) {
            char buf[12];
            int pos_min, pos_sec, dur_min, dur_sec;
            pos_min = (player_info.current_position/1000) / 60;
            if( pos_min > 99 ) {
                pos_min = 99;
            }
            pos_sec = (player_info.current_position/1000) % 60;
            if( player_info.song ) {
                if( player_info.song->play_length == -1 ) {
                    snprintf(buf, 12, "%02d:%02d", pos_min, pos_sec);
                } else {
                    dur_min = (player_info.song->play_length/1000) / 60;
                    if( dur_min > 99 ) {
                        dur_min = 99;
                    }
                    dur_sec = (player_info.song->play_length/1000) % 60;
                    snprintf(buf, 12, "%02d:%02d-%02d:%02d", pos_min, pos_sec, dur_min, dur_sec );
                }
                draw_string_monospaced_empeg( display_info.screen, buf, 26, 0, 4 );
            }
            if( player_info.song && player_info.song->play_length != 0 ) {
                int pos = WIDTH*player_info.current_position/player_info.song->play_length;
                vfdlib_drawPointClipped( display_info.screen, pos, 24, 3);
            }
        } else {
            char buf[12];
            int dur_min, dur_sec;
            dur_min = (song->play_length/1000) / 60;
            if( dur_min > 99 ) {
                dur_min = 99;
            }
            dur_sec = (song->play_length/1000) % 60;
            snprintf(buf, 12, "%02d:%02d", dur_min, dur_sec );
            draw_string_monospaced_empeg( display_info.screen, buf, 26, 0, 4 );
        }
    }
}
#endif

#ifdef EMPEG
void draw_empeg_display( void ) {

    vfdlib_clear( display_info.screen, 0 );
#ifdef ADVENTURE
    switch( display_info.cur_screen ) {
        case EMPEG_SCREEN_PLAY:
            if( player_info.device ) {
                int volume = player_info.device->volume[0];
                if( volume >= 0 && volume <= 100 ) {
                    char *volume_string;
                    asprintf( &volume_string, "Vol:%3d", volume );
                    draw_string_monospaced_empeg( display_info.screen, volume_string, 0, WIDTH-7*4, 4 );
                    free( volume_string );
                }
            }
            {
                char *buffer_string;
                asprintf( &buffer_string, "Buf:%5.2fs", (float)frame_buffer.pcm_size / 44100 / 2 / 2 );
                draw_string_monospaced_empeg( display_info.screen, buffer_string, 0, 0, 4 );
            }
            draw_song_empeg( player_info.song, TRUE );
            break;
        case EMPEG_SCREEN_NAVIGATION_1:
            {
                int string_width;
                int x;
                string_width = vfdlib_getTextWidth( "Currently Playing", 0 );
                x = (WIDTH - string_width) / 2;
                draw_string_empeg( display_info.screen, "Currently Playing", 0, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Playlist", 0 );
                x = 0;
                draw_string_empeg( display_info.screen, "Playlist", 12, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "", 0 );
                x = WIDTH - string_width;
                draw_string_empeg( display_info.screen, "", 12, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Next Menu", 0 );
                x = (WIDTH - string_width) / 2;
                draw_string_empeg( display_info.screen, "Next Menu", 24, x, WIDTH-x );
            }
            break;
        case EMPEG_SCREEN_NAVIGATION_2:
            {
                int string_width;
                int x;
                string_width = vfdlib_getTextWidth( "Quit Player", 0 );
                x = (WIDTH - string_width) / 2;
                draw_string_empeg( display_info.screen, "Quit Player", 0, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Delete", 0 );
                x = 0;
                draw_string_empeg( display_info.screen, "Delete", 9, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "The Masterlist", 0 );
                x = 0;
                draw_string_empeg( display_info.screen, "The Masterlist", 15, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Set Display", 0 );
                x = WIDTH - string_width;
                draw_string_empeg( display_info.screen, "Set Display", 9, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Brightness", 0 );
                x = WIDTH - string_width;
                draw_string_empeg( display_info.screen, "Brightness", 15, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Next Menu", 0 );
                x = (WIDTH - string_width) / 2;
                draw_string_empeg( display_info.screen, "Next Menu", 24, x, WIDTH-x );
            }
            break;
        case EMPEG_SCREEN_PLAY_SUBMENU:
            {
                int string_width;
                int x;
                if( player_info.state == STATE_PLAY ) {
                    string_width = vfdlib_getTextWidth( "Pause", 0 );
                    x = (WIDTH - string_width) / 2;
                    draw_string_empeg( display_info.screen, "Pause", 0, x, WIDTH-x );
                } else {
                    string_width = vfdlib_getTextWidth( "Play", 0 );
                    x = (WIDTH - string_width) / 2;
                    draw_string_empeg( display_info.screen, "Play", 0, x, WIDTH-x );
                }

                string_width = vfdlib_getTextWidth( "", 0 );
                x = 0;
                draw_string_empeg( display_info.screen, "", 12, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Song Info", 0 );
                x = WIDTH - string_width;
                draw_string_empeg( display_info.screen, "Song Info", 12, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Return to", 0 );
                x = (WIDTH - string_width) / 2;
                draw_string_empeg( display_info.screen, "Return to", 18, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Currently Playing", 0 );
                x = (WIDTH - string_width) / 2;
                draw_string_empeg( display_info.screen, "Currently Playing", 24, x, WIDTH-x );
            }
            break;
        case EMPEG_SCREEN_PLAYLIST_SUBMENU:
            {
                int string_width;
                int x;
                string_width = vfdlib_getTextWidth( "Remove from Playlist", 0 );
                x = (WIDTH - string_width) / 2;
                draw_string_empeg( display_info.screen, "Remove from Playlist", 0, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "", 0 );
                x = 0;
                draw_string_empeg( display_info.screen, "", 12, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Song Info", 0 );
                x = WIDTH - string_width;
                draw_string_empeg( display_info.screen, "Song Info", 12, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Return to", 0 );
                x = (WIDTH - string_width) / 2;
                draw_string_empeg( display_info.screen, "Return to", 18, x, WIDTH-x );

                string_width = vfdlib_getTextWidth( "Playlist", 0 );
                x = (WIDTH - string_width) / 2;
                draw_string_empeg( display_info.screen, "Playlist", 24, x, WIDTH-x );
            }
            break;
        case EMPEG_SCREEN_PLAYLIST:
            {
                int i = 0;
                song_queue_entry_t *cur_queue_entry;
                song_info_t *cur_song;
                char *status;
                int x;

                cur_queue_entry = song_queue.head;
                while( cur_queue_entry != NULL && i != song_queue.selected ) {
                    cur_queue_entry = cur_queue_entry->next;
                    i++;
                }
                asprintf( &status, "Song %d of %d in playlist", i+1, song_queue.size );
                x = (WIDTH - vfdlib_getTextWidth( status, 0 )) / 2;
                draw_string_empeg( display_info.screen, status, 0, x, WIDTH-x );
                draw_string_empeg( display_info.screen, "<<<", 0, 0, WIDTH );
                draw_string_empeg( display_info.screen, ">>>", 0, WIDTH-4*3, 4*3 );
                free(status);

                if( cur_queue_entry != NULL ) {
                    cur_song = cur_queue_entry->song_info;
                    draw_song_empeg( cur_song, FALSE );
                }
            }
            break;
        case EMPEG_SCREEN_PLAY_SONG_INFO:
        case EMPEG_SCREEN_PLAYLIST_SONG_INFO:
            {
                char *filename;
                float rating, avg, std_dev;
                int play_count, skip_count;
                char *line_buffer;
                int x;

                if( display_info.cur_screen == EMPEG_SCREEN_PLAY_SONG_INFO ) {
                    filename = player_info.song->filename;
                    rating = get_rating( player_info.song->stat );
                    play_count = player_info.song->stat.play_count;
                    skip_count = player_info.song->stat.skip_count;
                } else {
                    int i = 0;
                    song_queue_entry_t *cur_queue_entry;
                    song_info_t *cur_song;

                    cur_queue_entry = song_queue.head;
                    while( cur_queue_entry != NULL && i != song_queue.selected ) {
                        cur_queue_entry = cur_queue_entry->next;
                        i++;
                    }

                    cur_song = cur_queue_entry->song_info;

                    filename = cur_song->filename;
                    rating = get_rating( cur_song->stat );
                    play_count = cur_song->stat.play_count;
                    skip_count = cur_song->stat.skip_count;
                }

                draw_string_empeg( display_info.screen, filename, 0, 0, WIDTH );

                draw_string_empeg( display_info.screen, "Stats:", 6, 0, WIDTH );
                x = WIDTH - vfdlib_getTextWidth( "(current/avg/std dev)", 0 );
                draw_string_empeg( display_info.screen, "(current/avg/std dev)", 6, x, WIDTH-x );

                draw_string_empeg( display_info.screen, "Rating:", 12, 0, WIDTH );
                draw_string_empeg( display_info.screen, "Play Count:", 18, 0, WIDTH );
                draw_string_empeg( display_info.screen, "Skip Count:", 24, 0, WIDTH );

                avg = database_info.sum / database_info.song_count;
                std_dev = sqrt( fabs(database_info.sqr_sum / database_info.song_count - avg*avg) );
                asprintf( &line_buffer, "% 6.3f/% 6.3f/% 6.3f", rating, avg, std_dev );
                draw_string_monospaced_empeg( display_info.screen, line_buffer, 12, 10*4, 4 );
                free( line_buffer );

                avg = (double)database_info.play_sum / database_info.song_count;
                std_dev = sqrt( fabs((double)database_info.play_sqr_sum / database_info.song_count - avg*avg) );
                asprintf( &line_buffer, "% 6d/% 6.3f/% 6.3f", play_count, avg, std_dev );
                draw_string_monospaced_empeg( display_info.screen, line_buffer, 18, 10*4, 4 );
                free( line_buffer );

                avg = (double)database_info.skip_sum / database_info.song_count;
                std_dev = sqrt( fabs((double)database_info.skip_sqr_sum / database_info.song_count - avg*avg) );
                asprintf( &line_buffer, "% 6d/% 6.3f/% 6.3f", skip_count, avg, std_dev );
                draw_string_monospaced_empeg( display_info.screen, line_buffer, 24, 10*4, 4 );
                free( line_buffer );
            }
            break;
        case EMPEG_SCREEN_QUIT:
            draw_string_empeg( display_info.screen, "Squash is shutting down, please wait", 6, 0, WIDTH );
            break;
        case EMPEG_SCREEN_DELETE_MASTERLIST:
            draw_string_empeg( display_info.screen, "Masterlist deleted,", 6, 0, WIDTH );
            draw_string_empeg( display_info.screen, "Press any key to return", 12, 0, WIDTH );
            break;
        case EMPEG_SCREEN_SET_DISPLAY_BRIGHTNESS:
            {
                char *line_buffer;
                asprintf( &line_buffer, "Current brightness << %d >>", display_info.brightness );
                draw_string_empeg( display_info.screen, line_buffer, 12, 0, WIDTH );
                free( line_buffer );
            }
            break;
        /* We shouldn't actually get to this */
        case EMPEG_SCREEN_NONE:
            draw_string_empeg( display_info.screen, "Blackhole", 6, 0, WIDTH );
            break;
    }
#else
    if( display_info.in_menu ) {
        int i, first, last, selection;
        selection = display_info.menu_selection;
        first = 0;
        last = menu_list[ display_info.cur_screen ].item_count;
        if( last - first > 5 ) {
            if( selection - first > 2 ) {
                first = selection - 2;
                if( last - first < 6 ) {
                    first = last - 5;
                }
            }
        }
        if( last - first > 5 ) {
            last = first + 5;
        }
        if( first == 0 ) {
            vfdlib_drawLineHorizClipped( display_info.screen, 0, 0, WIDTH, 2);
        }
        if( last == menu_list[ display_info.cur_screen ].item_count ) {
            vfdlib_drawLineHorizClipped( display_info.screen, 1+(last-first)*6, 0, WIDTH, 2);
        }
        draw_string_empeg( display_info.screen, ">", 1 + 6 * (selection-first), 0, WIDTH );
        for( i = first; i < last; i++ ) {
            draw_string_empeg( display_info.screen, menu_list[ display_info.cur_screen ].items[i].title, 1+(i-first)*6, 10 + 10*menu_list[ display_info.cur_screen ].items[i].indentation, WIDTH );
        }
    } else {
        switch( display_info.cur_screen ) {
            case EMPEG_SCREEN_PLAY:
                if( player_info.device ) {
                    int volume = player_info.device->volume[0];
                    if( volume >= 0 && volume <= 100 ) {
                        char *volume_string;
                        asprintf( &volume_string, "Vol:%3d", volume );
                        draw_string_monospaced_empeg( display_info.screen, volume_string, 0, WIDTH-7*4, 4 );
                        free( volume_string );
                    }
                }
                draw_song_empeg( player_info.song, TRUE );
                break;
            case EMPEG_SCREEN_PLAY_RATE_SONG:
                draw_stars_empeg( player_info.song );
                break;
            case EMPEG_SCREEN_PLAYLIST_RATE_SONG:
                {
                    int i = 0;
                    song_queue_entry_t *cur_queue_entry;
                    song_info_t *cur_song;

                    cur_queue_entry = song_queue.head;
                    while( cur_queue_entry != NULL && i != song_queue.selected ) {
                        cur_queue_entry = cur_queue_entry->next;
                        i++;
                    }

                    cur_song = cur_queue_entry->song_info;

                    if( cur_song ) {
                        draw_stars_empeg( cur_song );
                    }
                }
                break;
            case EMPEG_SCREEN_GLOBAL_SONG_INFO:
                {
                    char *line_buffer;
                    float avg, std_dev;
                    int x;

                    draw_string_empeg( display_info.screen, "Stats:", 6, 0, WIDTH );
                    draw_string_empeg( display_info.screen, "(avg", 6, 11*4, WIDTH-11*4 );
                    draw_string_empeg( display_info.screen, "/", 6, 19*4, WIDTH-19*4 );
                    x = 29*4 - vfdlib_getTextWidth( "std dev)", 2 ) + 1;
                    draw_string_empeg( display_info.screen, "stddev)", 6, x, WIDTH-x );
                    draw_string_empeg( display_info.screen, "Rating:", 12, 0, WIDTH );
                    draw_string_empeg( display_info.screen, "Play Count:", 18, 0, WIDTH );
                    draw_string_empeg( display_info.screen, "Skip Count:", 24, 0, WIDTH );

                    avg = database_info.sum / database_info.song_count;
                    std_dev = sqrt( fabs(database_info.sqr_sum / database_info.song_count - avg*avg) );
                    asprintf( &line_buffer, "% 9.6f/% 9.6f", avg, std_dev );
                    draw_string_monospaced_empeg( display_info.screen, line_buffer, 12, 10*4, 4 );
                    free( line_buffer );

                    avg = (double)database_info.play_sum / database_info.song_count;
                    std_dev = sqrt( fabs((double)database_info.play_sqr_sum / database_info.song_count - avg*avg) );
                    asprintf( &line_buffer, "% 9.6f/% 9.6f", avg, std_dev );
                    draw_string_monospaced_empeg( display_info.screen, line_buffer, 18, 10*4, 4 );
                    free( line_buffer );

                    avg = (double)database_info.skip_sum / database_info.song_count;
                    std_dev = sqrt( fabs((double)database_info.skip_sqr_sum / database_info.song_count - avg*avg) );
                    asprintf( &line_buffer, "% 9.6f/% 9.6f", avg, std_dev );
                    draw_string_monospaced_empeg( display_info.screen, line_buffer, 24, 10*4, 4 );
                    free( line_buffer );
                }
                break;
            case EMPEG_SCREEN_PLAY_SONG_INFO:
            case EMPEG_SCREEN_PLAYLIST_SONG_INFO:
                {
                    char *filename;
                    float rating;
                    int play_count, skip_count;
                    char *line_buffer;

                    if( display_info.cur_screen == EMPEG_SCREEN_PLAY_SONG_INFO ) {
                        filename = player_info.song->filename;
                        rating = get_rating( player_info.song->stat );
                        play_count = player_info.song->stat.play_count;
                        skip_count = player_info.song->stat.skip_count;
                    } else {
                        int i = 0;
                        song_queue_entry_t *cur_queue_entry;
                        song_info_t *cur_song;

                        cur_queue_entry = song_queue.head;
                        while( cur_queue_entry != NULL && i != song_queue.selected ) {
                            cur_queue_entry = cur_queue_entry->next;
                            i++;
                        }

                        cur_song = cur_queue_entry->song_info;

                        filename = cur_song->filename;
                        rating = get_rating( cur_song->stat );
                        play_count = cur_song->stat.play_count;
                        skip_count = cur_song->stat.skip_count;
                    }

                    draw_string_empeg( display_info.screen, filename, 0, 0, WIDTH );

                    draw_string_empeg( display_info.screen, "Stats:", 6, 0, WIDTH );

                    draw_string_empeg( display_info.screen, "Rating:", 12, 0, WIDTH );
                    draw_string_empeg( display_info.screen, "Play Count:", 18, 0, WIDTH );
                    draw_string_empeg( display_info.screen, "Skip Count:", 24, 0, WIDTH );

                    asprintf( &line_buffer, "% 6.3f", rating );
                    draw_string_monospaced_empeg( display_info.screen, line_buffer, 12, 10*4, 4 );
                    free( line_buffer );

                    asprintf( &line_buffer, "% 6d", play_count );
                    draw_string_monospaced_empeg( display_info.screen, line_buffer, 18, 10*4, 4 );
                    free( line_buffer );

                    asprintf( &line_buffer, "% 6d", skip_count );
                    draw_string_monospaced_empeg( display_info.screen, line_buffer, 24, 10*4, 4 );
                    free( line_buffer );
                }
                break;
            case EMPEG_SCREEN_PLAYLIST:
                {
                    int i = 0;
                    song_queue_entry_t *cur_queue_entry;
                    song_info_t *cur_song;
                    char *status;
                    int x;

                    cur_queue_entry = song_queue.head;
                    while( cur_queue_entry != NULL && i != song_queue.selected ) {
                        cur_queue_entry = cur_queue_entry->next;
                        i++;
                    }
                    asprintf( &status, "Song %d of %d in playlist", i+1, song_queue.size );
                    x = (WIDTH - vfdlib_getTextWidth( status, 0 )-1) / 2;
                    draw_string_empeg( display_info.screen, status, 0, x, WIDTH-x );
                    draw_string_empeg( display_info.screen, "<<<", 0, 0, WIDTH );
                    draw_string_empeg( display_info.screen, ">>>", 0, WIDTH-4*3, 4*3 );
                    free(status);

                    if( cur_queue_entry != NULL ) {
                        cur_song = cur_queue_entry->song_info;
                        draw_song_empeg( cur_song, FALSE );
                    }
                }
                break;
            case EMPEG_SCREEN_QUIT:
                draw_string_empeg( display_info.screen, "Are you sure you would like to quit?", 6, 0, WIDTH );
                draw_string_empeg( display_info.screen, "(Press the knob to confirm)", 12, 0, WIDTH );
                break;
            case EMPEG_SCREEN_SET_DISPLAY_BRIGHTNESS:
                {
                    char *line_buffer;
                    asprintf( &line_buffer, "Current brightness << %d >>", display_info.brightness );
                    draw_string_empeg( display_info.screen, line_buffer, 12, 0, WIDTH );
                    free( line_buffer );
                }
                break;
            case EMPEG_SCREEN_SET_RATING_BIAS:
                {
                    char *line_buffer;
                    asprintf( &line_buffer, "Current Bias << %3d%% >>", (int)(100*config.db_manual_rating_bias) );
                    draw_string_empeg( display_info.screen, line_buffer, 9, 0, WIDTH );
                    free( line_buffer );
                    draw_string_empeg( display_info.screen, "(Higher values bias manual ratings", 15, 0, WIDTH );
                    draw_string_empeg( display_info.screen, "versus automatic rating)", 21, 0, WIDTH );
                }
                break;
            default:
                draw_string_empeg( display_info.screen, "Blackhole", 6, 0, WIDTH );
                break;
        }
    }
#endif
    ioctl(display_info.screen_fd, _IO('d', 0));
}
#undef WIDTH
#endif

#ifndef NO_NCURSES
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
    int rating_left;
    int rating_title_left;
    key_set_t title_set = (key_set_t) { (char *[]){"title"}, 1 };
    key_set_t artist_set = (key_set_t) { (char *[]){"artist", "artist_original"}, 2 };
    key_set_t album_set = (key_set_t) { (char *[]){"album", "album_original"}, 2 };
    key_set_t tracknumber_set = (key_set_t) { (char *[]){"tracknumber"}, 1 };

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

    if( display_info.focus == WIN_NOW_PLAYING ) {
        wattron( win, COLOR_PAIR(TEXT_WHITE) | A_BOLD );
        mvwprintw( win, 0, 1, "*" );
        wattroff( win, COLOR_PAIR(TEXT_WHITE) | A_BOLD );
    }

    /* Get Song */
    cur_song = player_info.song;
    if( !cur_song || cur_song->play_length == -1 ) {
        cur_duration = -1;
    } else {
        cur_duration = cur_song->play_length / 1000;
    }
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
    {
        int time_width = num_chars(cur_position / 60) + 3;
        if( cur_duration != -1 ) {
            time_width += num_chars(cur_duration / 60) + 4;
        }
        time_left = win_width - time_width - 1;
    }

    rating_left = win_width - strlen("OOOOO") - 1;
    rating_title_left = win_width - strlen("Manual Rating:") - 1;

    if( time_left > rating_left ) {
        time_left = rating_left;
    } else {
        rating_left = time_left;
    }
    if( time_left > rating_title_left ) {
        time_left = rating_title_left;
        rating_left = time_left;
    } else {
        rating_title_left = time_left;
    }

    /* Print song information */
    draw_meta_string( win, cur_song, title_set, 1, 10, rating_title_left-1 - 10 );
    draw_meta_string( win, cur_song, artist_set, 2, 10, rating_left-1 - 10 );
    draw_meta_string( win, cur_song, album_set, 3, 10, time_left-1 - 10 );
    draw_meta_string( win, cur_song, tracknumber_set, 4, 10, 9 );

    {
        int i;
        int rating = cur_song->stat.manual_rating;
        mvwprintw( win, 1, rating_title_left, "Manual Rating:" );
        if( rating == -1 ) {
            mvwprintw( win, 2, rating_left, "N/R" );
        } else {
            for( i = 0; i < rating/2; i++ ) {
                mvwaddch( win, 2, rating_left + i, 'O' );
            }
            if( rating % 2 != 0 ) {
                mvwaddch( win, 2, rating_left + i, 'o' );
                i++;
            }
            for( ; i < 5; i++ ) {
                mvwaddch( win, 2, rating_left + i, '.' );
            }
        }
    }

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
    }

    wattron( win, text_color );
    mvwaddch( win, 0, rating_title_left - 1, ACS_TTEE );
    mvwaddch( win, 1, rating_title_left - 1, ACS_VLINE );
    mvwaddch( win, 2, rating_title_left - 1, ACS_VLINE );
    mvwaddch( win, 3, rating_title_left - 1, ACS_VLINE );
    mvwaddch( win, 4, rating_title_left - 1, ACS_BTEE );
    wattroff( win, text_color );

    if( cur_duration == -1 ) {
        /* Print Time Information */
        mvwprintw(    win, 3, time_left, "%d:%02d",
            (int)(cur_position / 60),
            (int)(cur_position % 60),
            (int)(cur_duration / 60),
            (int)(cur_duration % 60) );
    } else {
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
#endif

#ifndef NO_NCURSES
/*
 * Draw the playlist window
 */
void draw_list( int which_window ) {
    WINDOW *win;
    int win_height, win_width;
    int col_left[ 3 ];
    int col_width;
    int list_height, start_index, max_size;
    int text_color, select_color;
    int i;
    bool have_focus;
    song_queue_t *queue;
    char *header;
    key_set_t title_set = (key_set_t) { (char *[]){"title"}, 1 };
    key_set_t artist_set = (key_set_t) { (char *[]){"artist", "artist_original"}, 2 };
    key_set_t album_set = (key_set_t) { (char *[]){"album", "album_original"}, 2 };

    switch( which_window ) {
        case WIN_PLAYLIST:
            queue = &song_queue;
            header = "Up Next";
            text_color = COLOR_PAIR( TEXT_BLUE ) | A_BOLD;
            break;
        case WIN_PASTLIST:
            header = "Past Items";
            queue = &past_queue;
            text_color = COLOR_PAIR( TEXT_CYAN );
            break;
        default:
            return;
    }

    have_focus = which_window == display_info.focus;

    /* Set the window */
    if( (win = display_info.window[which_window].window) == NULL ) {
        return;
    }


    /* Get window size */
    getmaxyx( win, win_height, win_width );

    /* Clear the screen */
    werase( win );

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
    mvwprintw( win, 0, 2, header );
    mvwaddstr( win, 1, col_left[0], "Artist" );
    mvwaddstr( win, 1, col_left[1], "Title" );
    mvwaddstr( win, 1, col_left[2], "Album" );

    /* Reset item color */
    wattroff( win, text_color );

    if( have_focus ) {
        wattron( win, COLOR_PAIR(TEXT_WHITE) | A_BOLD );
        mvwprintw( win, 0, 1, "*" );
        wattroff( win, COLOR_PAIR(TEXT_WHITE) | A_BOLD );
    }

    /* Setup row information */
    list_height = win_height - 3;

    /* Process Playlist */
    if( queue->size <= 0 ) {
        wattron( win, COLOR_PAIR(TEXT_RED) );
        if( display_info.state == SYSTEM_LOADING ) {
            mvwprintw( win, 3, (win_width / 2) - 14, "-- Database loading...... --" );
        } else {
            mvwprintw( win, 3, (win_width / 2) - 14, "-- No Entries in Playlist --" );
        }
        wattroff( win, COLOR_PAIR(TEXT_RED) );
    } else {
        song_queue_entry_t *cur_queue_entry;

        /* If our size is bigger than it will eventually be
           we are as big as we are going to get, so
           if our selection is too big clip it */
        if ( queue->size >= queue->wanted_size && queue->selected >= queue->size ) {
            queue->selected = queue->size - 1;
        }

        /* Update the number of rows to display */
        if( queue->size <= list_height ) {
            list_height = queue->size;
            start_index = 0;
        } else {
            start_index = queue->selected - list_height / 2;
            max_size = queue->size > queue->wanted_size ? queue->size : queue->wanted_size;

            /* Check boundries */
            if( start_index < 0 ) {
                start_index = 0;
            } else if( (start_index + list_height) > max_size ) {
                start_index = max_size - list_height;
            }

            /* Print Scroll Arrows */
            if( start_index > 0 ) {
                mvwaddch( win, 2, win_width - 2, '^' );
            }
            if( (start_index + list_height) < queue->size ) {
                mvwaddch( win, win_height - 2, win_width - 2, 'v' );
            }
        }

        /*
         * Display playlist
         */
        cur_queue_entry = queue->head;
        select_color = COLOR_PAIR( TEXT_BLUE_SELECTED ) | A_BOLD;

        /* Draw Selected Line */
        if( queue->selected >= 0 && have_focus ) {
            wattron( win, COLOR_PAIR(TEXT_BLUE_BACKGROUND) );
            wmove( win, queue->selected - start_index + 2, 1 );
            whline( win, 0, win_width - 2 );
            wattroff( win, COLOR_PAIR(TEXT_BLUE_BACKGROUND) );
        }

        /* Move to the correct queue entry */
        for( i = 0; i < start_index && cur_queue_entry != NULL; i++ ) {
            cur_queue_entry = cur_queue_entry->next;
        }

        /* Print the queue */
        for( i = 0; i < list_height; i++ ) {
            /* Allow for incorrect size information */
            if( cur_queue_entry == NULL ) {
                break;
            }

            if( (i + start_index) == queue->selected && have_focus ) {
                wattron( win, select_color );
            }

            /* Print playlist information */
            draw_meta_string( win, cur_queue_entry->song_info, artist_set, i + 2, col_left[0], col_width );
            draw_meta_string( win, cur_queue_entry->song_info, title_set, i + 2, col_left[1], col_width );
            draw_meta_string( win, cur_queue_entry->song_info, album_set, i + 2, col_left[2], col_width );

            if( (i + start_index) == queue->selected && have_focus ) {
                wattroff( win, select_color );
            }

            /* Load the next entry */
            cur_queue_entry = cur_queue_entry->next;
        }
    }

    /* Refresh Changes */
    wrefresh( win );
}
#endif

#ifndef NO_NCURSES
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
#endif

#ifndef NO_NCURSES
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
    mvwaddstr( win, 1 , 38, "m/M Adjust Rating" );

    /* Print Keys */
    wattron( win, A_BOLD );
    mvwaddch( win, 0, 2, 'Q' );
    mvwaddch( win, 1, 5, 'c' );
    mvwaddch( win, 0, 14, 'P' );
    mvwaddch( win, 1, 15, 't' );
    mvwaddch( win, 0, 26, 'S' );
    mvwaddch( win, 1, 26, 'E' );
    mvwaddch( win, 0, 38, 'I' );
    mvwaddch( win, 1, 38, 'm' );
    mvwaddch( win, 1, 40, 'M' );
    wattroff( win, A_BOLD );

    /* Refresh Changes */
    wrefresh( win );
}
#endif

#ifndef NO_NCURSES
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
    song_queue_t *queue = NULL;
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
    switch( display_info.focus ) {
        case WIN_NOW_PLAYING:
            /* selected is the now playing window */
            song = player_info.song;
            break;
        case WIN_PLAYLIST:
            queue = &song_queue;
        case WIN_PASTLIST:
            if( queue == NULL ) {
                queue = &past_queue;
            }
            /* currently not selecting anything (delete at the end of the list) */
            if( queue->size - 1 < queue->selected ) {
                song = NULL;
            } else {
                /* Otherwise go and find the entry on the queue */
                queue_entry = queue->head;
                for( i = 0; i < queue->selected; i++ ) {
                    queue_entry = queue_entry->next;
                }
                if( queue_entry == NULL ) {
                    song = NULL;
                } else {
                    song = queue_entry->song_info;
                }
            }
            break;
        default:
            song = NULL;
            break;
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
#endif

#ifndef NO_NCURSES
/*
 * Draws all values for a set of keys within a song entry.
 * Will also take into account limited display space (units are characters).
 */
void draw_meta_string( WINDOW *win, song_info_t *song, key_set_t key_set, int top, int left, int width ) {
    char *line_buffer;
    char *cur_format;
    meta_key_t *meta_data;
    int line_width;
    int i, j;

    /* Allow room for trailing semi-colon */
    width += 2;

    /* Allocate space for the buffer */
    squash_malloc( line_buffer, width + 1 );

    /* Build the string */
    line_width = 0;
    line_buffer[0] = '\0';
    cur_format = "%s; ";

    for( i = 0; (i < key_set.key_count) && (line_width < width); i++ ) {
        meta_data = get_meta_data( song, key_set.keys[i] );
        if( meta_data == NULL ) {
            continue;
        }
        for( j = 0; (j < meta_data->value_count) && (line_width < width); j++ ) {
            /* Add the current value to the line */
            line_width += snprintf( line_buffer + line_width, width - line_width + 1, cur_format, meta_data->values[j] );
        }
    }

    if( line_width == 0 ) {
        line_width += snprintf( line_buffer, width + 1, "[Unknown]" );
    } else {
        /* remove trailing "; " */
        if( line_width <= width ) {
            line_buffer[ line_width - 2 ] = '\0';
            line_width -= 2;
        }
        width -= 2;
    }

    /* Show elipse if required */
    if( line_width - 2 > width ) {
        line_buffer[ width ] = '\0';
        line_buffer[ width - 1 ] = '.';
        line_buffer[ width - 2 ] = '.';
        line_buffer[ width - 3 ] = '.';
    }

    /* Print the line */
    mvwprintw( win, top, left, line_buffer );

    /* Cleanup */
    squash_free( line_buffer );
}
#endif

#ifdef EMPEG
/*
 * Draws an arbitrary string to the display using spacing pixels between each charater.
 * (numbers have max width of 4, and characters have a max width of 6)
 */
void draw_string_monospaced_empeg( char *buffer, char *string, int top, int left, int spacing ) {
    int x, width;
    char tmp[2] = " ";
    for( x = left; *string != '\0'; x += spacing, string++ ) {
        tmp[0] = *string;
        width = vfdlib_getTextWidth( tmp, 2 );
        vfdlib_drawText( buffer, tmp, x+(spacing-width+1)/2, top, 2, 3);
    }
}
#endif

#ifdef EMPEG
/*
 * Draws an arbitrary string.
 * Will also take into account limited display space (units are pixels).
 * Assumes the largest character is 6 pixels.
 * (to guess how few characters we need to remove when we are wrong).
 */
void draw_string_empeg( char *buffer, char *string, int top, int left, int width ) {
    char *line_buffer;
    int line_width, cur_width;
    int trim;

    line_buffer = strdup( string );
    line_width = strlen( string );
    cur_width = vfdlib_getTextWidth( line_buffer, 2 ) - 1;
    while( cur_width > width ) {
        trim = (cur_width - width + 5)/6;
        if( trim < 1 ) {
            trim = 1;
        }
        line_width -= trim;
        line_buffer[ line_width ] = '\0';
        cur_width = vfdlib_getTextWidth( line_buffer, 2 ) - 1;
    }
    vfdlib_drawText( buffer, line_buffer, left, top, 2, 3);

    /* Cleanup */
    squash_free( line_buffer );
}
#endif

#ifdef EMPEG
/*
 * Draws all values for a set of keys within a song entry.
 * Will also take into account limited display space (units are pixels).
 */
void draw_meta_string_empeg( char *buffer, song_info_t *song, key_set_t key_set, int top, int left, int width ) {
    char *line_buffer;
    meta_key_t *meta_data;
    int line_width; /* number of characters */
    int line_pixel_width; /* number of pixels */
    int line_alloc; /* number of characters allocated, not including null */
    int i, j, k;
    char cur_char[2] = " ";
    int space_width, semicolon_width, dot_width;
    bool added_trail;

    /* If we can't even fit the elipses, just don't print anything */
    if( vfdlib_getTextWidth( "...", 2 ) - 1 > width ) {
        return;
    }
    space_width = vfdlib_getTextWidth( " ", 2 );
    semicolon_width = vfdlib_getTextWidth( ";", 2 );
    dot_width = vfdlib_getTextWidth( ".", 2 );

    /* Allocate space for the buffer (assume each character is 1 pixel, and make room for the elipses/trailing "; ") */
    line_alloc = width + 3;
    squash_calloc( line_buffer, line_alloc+1, 1 );

    /* Build the string */
    line_width = 0;
    line_pixel_width = 0;
    added_trail = FALSE;

    for( i = 0; (i < key_set.key_count) && (line_width < line_alloc); i++ ) {
        meta_data = get_meta_data( song, key_set.keys[i] );
        if( meta_data == NULL ) {
            continue;
        }
        for( j = 0; (j < meta_data->value_count) && (line_width < line_alloc); j++ ) {
            /* Add each character to the line_buffer */
            for( k = 0; (meta_data->values[j][k] != '\0') && (line_width < line_alloc); k++ ) {
                line_buffer[line_width] = meta_data->values[j][k];
                line_width++;
                cur_char[0] = meta_data->values[j][k];
                line_pixel_width += vfdlib_getTextWidth( cur_char, 2 );
            }
            /* Add a semicolon and space */
            line_buffer[line_width] = ';';
            line_width++;
            line_pixel_width += semicolon_width;
            line_buffer[line_width] = ' ';
            line_width++;
            line_pixel_width += space_width;
            added_trail = TRUE;
        }
    }
    /* Remove the trailing "; " */
    if( added_trail ) {
        line_buffer[line_width - 2] = '\0';
        line_width -= 2;
        line_pixel_width -= semicolon_width;
        line_pixel_width -= space_width;
    }

    /* Remove the extra pixel spacing on the end */
    line_pixel_width--;

    /* If we are overflowed, first add the elipses */
    if( line_pixel_width > width ) {
        line_buffer[ line_width ] = '.';
        line_width++;
        line_pixel_width += dot_width;
        line_buffer[ line_width ] = '.';
        line_width++;
        line_pixel_width += dot_width;
        line_buffer[ line_width ] = '.';
        line_width++;
        line_pixel_width += dot_width;

        /* While we are overflown, move the elipses up one. */
        while( line_pixel_width > width && line_width - 4 > 0 ) {
            cur_char[0] = line_buffer[ line_width - 4 ];
            line_pixel_width -= vfdlib_getTextWidth( cur_char, 2 );
            line_buffer[ line_width - 4 ] = '.';
            line_pixel_width += dot_width;
            line_buffer[ line_width - 1 ] = '\0';
            line_pixel_width -= dot_width;
            line_width--;
        }
    }
    vfdlib_drawText( buffer, line_buffer, left, top, 2, 3);

    /* Cleanup */
    squash_free( line_buffer );
}
#endif

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
