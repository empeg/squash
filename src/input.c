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
#include "display.h"    /* for draw_screen() and set_display_info_brightness() */
#include "player.h"     /* for player_queue_command() */
#include "database.h"   /* for clear_song_meta() and load_meta_data() */
#include "stat.h"       /* for feedback() */
#include "sound.h"      /* for sound_adjust_volume */
#include "input.h"

#include "sys/time.h"   /* for struct timeval */

void do_remove_playlist_entry() {
#ifdef EMPEG
    int i;
    song_queue_entry_t *queue_entry;
    song_queue_entry_t *queue_last_entry;

    squash_wlock( database_info.lock );
    squash_lock( song_queue.lock );
    if( song_queue.selected < song_queue.size ) {
        queue_entry = song_queue.head;
        queue_last_entry = NULL;
        for( i = 0; i < song_queue.selected; i++ ) {
            queue_last_entry = queue_entry;
            queue_entry = queue_entry->next;
        }
        if( queue_last_entry != NULL ) {
            queue_last_entry->next = queue_entry->next;
        }
        if( queue_entry == song_queue.head ) {
            song_queue.head = queue_entry->next;
        }
        if( queue_entry == song_queue.tail ) {
            song_queue.tail = queue_last_entry;
        }
        feedback( queue_entry->song_info , -1);
        squash_free( queue_entry );
        song_queue.size--;
    }
    squash_unlock( song_queue.lock );
    squash_wunlock( database_info.lock );
    squash_broadcast( song_queue.not_full );
#endif
}

void do_song_rating_adjust( bool relative, int amount ) {
    song_info_t *song = NULL;

#ifdef EMPEG
    if( display_info.cur_screen == EMPEG_SCREEN_PLAY_RATE_SONG || display_info.cur_screen == EMPEG_SCREEN_PLAY ) {
#else
    if( display_info.focus == WIN_NOW_PLAYING ) {
#endif
        song = player_info.song;
#ifdef EMPEG
    } else if( 0 ) {
#else
    } else if( display_info.focus == WIN_PLAYLIST ) {
#endif
        int i;
        song_queue_entry_t *queue_entry;
        song_queue_entry_t *queue_last_entry;

        squash_lock( song_queue.lock );
        if( song_queue.selected < song_queue.size ) {
            queue_entry = song_queue.head;
            queue_last_entry = NULL;
            for( i = 0; i < song_queue.selected; i++ ) {
                queue_last_entry = queue_entry;
                queue_entry = queue_entry->next;
            }
            song = queue_entry->song_info;
        }
        squash_unlock( song_queue.lock );
#ifdef EMPEG
    } else if( 0 ) {
#else
    } else if( display_info.focus == WIN_PASTLIST ) {
#endif
        int i;
        song_queue_entry_t *queue_entry;
        song_queue_entry_t *queue_last_entry;

        squash_lock( past_queue.lock );
        if( past_queue.selected < past_queue.size ) {
            queue_entry = past_queue.head;
            queue_last_entry = NULL;
            for( i = 0; i < past_queue.selected; i++ ) {
                queue_last_entry = queue_entry;
                queue_entry = queue_entry->next;
            }
            song = queue_entry->song_info;
        }
        squash_unlock( past_queue.lock );
    }

    if( song ) {
        double rating;

        squash_wlock( database_info.lock );

        rating = get_rating( song->stat );
        database_info.sum -= rating;
        database_info.sqr_sum -= rating * rating;

        if( relative ) {
            song->stat.manual_rating += amount;
        } else {
            song->stat.manual_rating = amount;
        }

        if( song->stat.manual_rating < -1 ) {
            song->stat.manual_rating = -1;
        }
        if( song->stat.manual_rating > 10 ) {
            song->stat.manual_rating = 10;
        }

        rating = get_rating( song->stat );
        database_info.sum += rating;
        database_info.sqr_sum += rating * rating;

        song->stat.changed = TRUE;

        save_song( song );

        squash_wunlock( database_info.lock );
    }
}

/*
 * This routine is called whenever an event occurs.
 * An event is defined as a key on the remote being pressed,
 * a knob being turned.  Or else a key on the panel or the knob
 * being pressed and released.  Also, a key on the panel or
 * the knob being held (held set to TRUE).
 *
 * The button returned will always be IR*PRESSED and not IR*RELEASED
 * (for the panel buttons).  Even though it will be a IR*PRESSED event,
 * the event will only occur after IR*RELEASE has happened.
 *
 * The keys on the remote cannot be held so held will never be set
 * to TRUE for these keys.
 *
 * Also, when using the hijack kernel, you cannot have a knob held
 * event as it is captured (and just a knob event is hard to get).
 */
void process_ir_event( long button, bool held ) {
#ifdef EMPEG
#ifdef ADVENTURE
    bool done;

    done = TRUE;
    /* These buttons do not depend on the state you are in */
    switch( button ) {
        case IR_VOL_MINUS:
        case IR_KNOB_LEFT:
            squash_lock( player_info.lock );
            sound_adjust_volume( player_info.device, -2 );
            squash_unlock( player_info.lock );
            squash_broadcast( display_info.changed );
            break;
        case IR_VOL_PLUS:
        case IR_KNOB_RIGHT:
            squash_lock( player_info.lock );
            sound_adjust_volume( player_info.device, +2 );
            squash_unlock( player_info.lock );
            squash_broadcast( display_info.changed );
            break;
        case IR_PROG: /* PLAY/PAUSE */
            do_toggle_player_command();
            break;
        case IR_TRACK_MINUS:
            do_set_player_command( CMD_STOP );
            break;
        case IR_TRACK_PLUS:
            do_set_player_command( CMD_SKIP );
            break;
        default:
            done = FALSE;
            break;
    }

    if( done ) {
        return;
    }

    squash_lock( display_info.lock );
    switch( display_info.cur_screen ) {
        case EMPEG_SCREEN_PLAY:
            switch( button ) {
                case IR_TOP_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_PLAY_SUBMENU;
                    squash_broadcast( display_info.changed );
                    break;
                case IR_LEFT_BUTTON_PRESSED:
                    do_set_player_command( CMD_STOP );
                    break;
                case IR_RIGHT_BUTTON_PRESSED:
                    do_set_player_command( CMD_SKIP );
                    break;
                case IR_BOTTOM_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_NAVIGATION_1;
                    squash_broadcast( display_info.changed );
                    break;
                case IR_KNOB_PRESSED:
                    break;
                default:
                    break;
            }
            break;
        case EMPEG_SCREEN_NAVIGATION_1:
            switch( button ) {
                case IR_TOP_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_PLAY;
                    break;
                case IR_LEFT_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_PLAYLIST;
                    break;
                case IR_RIGHT_BUTTON_PRESSED:
                    //display_info.cur_screen = EMPEG_SCREEN_NONE;
                    break;
                case IR_BOTTOM_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_NAVIGATION_2;
                    break;
                case IR_KNOB_PRESSED:
                    break;
                default:
                    break;
            }
            squash_broadcast( display_info.changed );
            break;
        case EMPEG_SCREEN_NAVIGATION_2:
            switch( button ) {
                case IR_TOP_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_QUIT;
                    do_quit();
                    break;
                case IR_LEFT_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_DELETE_MASTERLIST;
                    if( config.db_masterlist_path ) {
                        unlink( config.db_masterlist_path );
                    }
                    break;
                case IR_RIGHT_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_SET_DISPLAY_BRIGHTNESS;
                    break;
                case IR_BOTTOM_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_NAVIGATION_1;
                    break;
                case IR_KNOB_PRESSED:
                    break;
                default:
                    break;
            }
            squash_broadcast( display_info.changed );
            break;
        case EMPEG_SCREEN_PLAYLIST:
            switch( button ) {
                case IR_TOP_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_PLAYLIST_SUBMENU;
                    squash_broadcast( display_info.changed );
                   break;
                case IR_LEFT_BUTTON_PRESSED:
                    squash_lock( song_queue.lock );
                    if( song_queue.selected > 0 ) {
                        song_queue.selected--;
                    }
                    squash_unlock( song_queue.lock );
                    squash_broadcast( display_info.changed );
                    break;
                case IR_RIGHT_BUTTON_PRESSED:
                    squash_lock( song_queue.lock );
                    if( song_queue.selected < song_queue.size-1 ) {
                        song_queue.selected++;
                    }
                    squash_unlock( song_queue.lock );
                    squash_broadcast( display_info.changed );
                    break;
                case IR_BOTTOM_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_NAVIGATION_1;
                    squash_broadcast( display_info.changed );
                    break;
                case IR_KNOB_PRESSED:
                    break;
                default:
                    break;
            }
            break;
        case EMPEG_SCREEN_PLAY_SONG_INFO:
            switch( button ) {
                case IR_TOP_BUTTON_PRESSED:
                case IR_LEFT_BUTTON_PRESSED:
                case IR_RIGHT_BUTTON_PRESSED:
                case IR_BOTTOM_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_PLAY;
                    squash_broadcast( display_info.changed );
                    break;
                case IR_KNOB_PRESSED:
                    break;
                default:
                    break;
            }
            break;
        case EMPEG_SCREEN_PLAYLIST_SONG_INFO:
            switch( button ) {
                case IR_TOP_BUTTON_PRESSED:
                case IR_LEFT_BUTTON_PRESSED:
                case IR_RIGHT_BUTTON_PRESSED:
                case IR_BOTTOM_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_PLAYLIST;
                    squash_broadcast( display_info.changed );
                    break;
                case IR_KNOB_PRESSED:
                    break;
                default:
                    break;
            }
            break;
        case EMPEG_SCREEN_PLAY_SUBMENU:
            switch( button ) {
                case IR_TOP_BUTTON_PRESSED:
                    do_toggle_player_command();
                    display_info.cur_screen = EMPEG_SCREEN_PLAY;
                    squash_broadcast( display_info.changed );
                    break;
                case IR_LEFT_BUTTON_PRESSED:
                    //display_info.cur_screen = EMPEG_SCREEN_NONE;
                    break;
                case IR_RIGHT_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_PLAY_SONG_INFO;
                    break;
                case IR_BOTTOM_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_PLAY;
                    break;
                case IR_KNOB_PRESSED:
                    break;
                default:
                    break;
            }
            squash_broadcast( display_info.changed );
            break;
        case EMPEG_SCREEN_PLAYLIST_SUBMENU:
            switch( button ) {
                case IR_TOP_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_PLAYLIST;
                    {
                        int i;
                        song_queue_entry_t *queue_entry;
                        song_queue_entry_t *queue_last_entry;

                        squash_wlock( database_info.lock );
                        squash_lock( song_queue.lock );
                        if( song_queue.selected < song_queue.size ) {
                            queue_entry = song_queue.head;
                            queue_last_entry = NULL;
                            for( i = 0; i < song_queue.selected; i++ ) {
                                queue_last_entry = queue_entry;
                                queue_entry = queue_entry->next;
                            }
                            if( queue_last_entry != NULL ) {
                                queue_last_entry->next = queue_entry->next;
                            }
                            if( queue_entry == song_queue.head ) {
                                song_queue.head = queue_entry->next;
                            }
                            if( queue_entry == song_queue.tail ) {
                                song_queue.tail = queue_last_entry;
                            }
                            feedback( queue_entry->song_info , -1);
                            squash_free( queue_entry );
                            song_queue.size--;
                        }
                        squash_unlock( song_queue.lock );
                        squash_wunlock( database_info.lock );
                        squash_broadcast( song_queue.not_full );
                        squash_broadcast( display_info.changed );
                    }
                    break;
                case IR_LEFT_BUTTON_PRESSED:
                    //display_info.cur_screen = EMPEG_SCREEN_NONE;
                    break;
                case IR_RIGHT_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_PLAYLIST_SONG_INFO;
                    squash_broadcast( display_info.changed );
                    break;
                case IR_BOTTOM_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_PLAYLIST;
                    squash_broadcast( display_info.changed );
                    break;
                case IR_KNOB_PRESSED:
                    break;
                default:
                    break;
            }
            squash_broadcast( display_info.changed );
            break;
        case EMPEG_SCREEN_QUIT:
            break;
        case EMPEG_SCREEN_DELETE_MASTERLIST:
            display_info.cur_screen = EMPEG_SCREEN_NAVIGATION_2;
            squash_broadcast( display_info.changed );
            break;
        case EMPEG_SCREEN_SET_DISPLAY_BRIGHTNESS:
            switch( button ) {
                case IR_TOP_BUTTON_PRESSED:
                    break;
                case IR_LEFT_BUTTON_PRESSED:
                    if( display_info.brightness >= 5 ) {
                        display_info.brightness -= 5;
                    }
                    set_display_brightness_empeg( display_info.brightness );
                    break;
                case IR_RIGHT_BUTTON_PRESSED:
                    if( display_info.brightness <= 95 ) {
                        display_info.brightness += 5;
                    }
                    set_display_brightness_empeg( display_info.brightness );
                    break;
                case IR_BOTTOM_BUTTON_PRESSED:
                    display_info.cur_screen = EMPEG_SCREEN_NAVIGATION_2;
                    break;
                case IR_KNOB_PRESSED:
                    break;
                default:
                    break;
            }
            squash_broadcast( display_info.changed );
            break;
        case EMPEG_SCREEN_NONE:
            break;
    }
    squash_unlock( display_info.lock );
#else
    bool done = FALSE;

    squash_lock( display_info.lock );

    if( button ==  IR_TOP_BUTTON_PRESSED && held ) {
        done = TRUE;
        switch( display_info.cur_screen ) {
            case EMPEG_SCREEN_SET_RATING_BIAS:
                database_info.stats_loaded = FALSE;
                start_song_picker();
                squash_signal( database_info.stats_finished );
                break;
            default:
                break;
        }

        if( display_info.cur_screen == EMPEG_SCREEN_PLAY ) {
            display_info.cur_screen = EMPEG_SCREEN_PLAYLIST;
        } else {
            display_info.cur_screen = EMPEG_SCREEN_PLAY;
        }
        display_info.in_menu = FALSE;
    }

    if( !done && display_info.in_menu ) {
        menu_list_t menu = menu_list[ display_info.cur_screen ];
        done = TRUE;
        switch( button ) {
            case IR_LEFT_BUTTON_PRESSED:
                /* This assumes that the zero'th item is go back or something similar */
                display_info.menu_selection = 0;
                display_info.in_menu = ! menu.items[ display_info.menu_selection ].leave_menu;
                display_info.cur_screen = menu.items[ display_info.menu_selection ].screen_on_press;
                display_info.menu_selection = menu_list[ display_info.cur_screen ].default_item;
                break;
            case IR_RIGHT_BUTTON_PRESSED:
            case IR_KNOB_PRESSED:
                switch( display_info.cur_screen ) {
                    case EMPEG_SCREEN_PLAYLIST:
                        if( display_info.menu_selection == 4 ) { /* Remove from playlist */
                            do_remove_playlist_entry();
                        }
                        break;
                    default:
                        break;
                }
                display_info.in_menu = ! menu.items[ display_info.menu_selection ].leave_menu;
                display_info.cur_screen = menu.items[ display_info.menu_selection ].screen_on_press;
                display_info.menu_selection = menu_list[ display_info.cur_screen ].default_item;
                break;
            case IR_TOP_BUTTON_PRESSED:
            case IR_KNOB_LEFT:
                if( display_info.menu_selection > 0 ) {
                    display_info.menu_selection--;
                }
                break;
            case IR_BOTTOM_BUTTON_PRESSED:
            case IR_KNOB_RIGHT:
                if( display_info.menu_selection+1 < menu.item_count ) {
                    display_info.menu_selection++;
                }
                break;
            default:
                break;
        }

    }

    if( !done ) {
        /* This handles buttons that do not change meaning based on your current screen
         * (mostly buttons on the remote) */
        done = TRUE;
        switch( button ) {
            case IR_VOL_MINUS:
                squash_lock( player_info.lock );
                sound_adjust_volume( player_info.device, -2 );
                squash_unlock( player_info.lock );
                break;
            case IR_VOL_PLUS:
                squash_lock( player_info.lock );
                sound_adjust_volume( player_info.device, +2 );
                squash_unlock( player_info.lock );
                break;
            case IR_PROG: /* PLAY/PAUSE */
                do_toggle_player_command();
                break;
            case IR_TRACK_MINUS:
                do_set_player_command( CMD_STOP );
                break;
            case IR_TRACK_PLUS:
                do_set_player_command( CMD_SKIP );
                break;
            default:
                done = FALSE;
                break;
        }
    }

    /* handle knob confirm in quit screen */
    if( !done && display_info.cur_screen == EMPEG_SCREEN_QUIT ) {
        done = TRUE;
        switch( button ) {
            case IR_KNOB_PRESSED:
                do_quit();
                break;
            default:
                display_info.in_menu = TRUE;
                display_info.cur_screen = EMPEG_SCREEN_MAIN_MENU;
                display_info.menu_selection = menu_list[ display_info.cur_screen ].default_item;
                break;
        }
    }

    /* handle going to the menu screen (we may also have to do some event, such as update statistics */
    if( !done ) {
        switch( button ) {
            case IR_BOTTOM_BUTTON_PRESSED:
            case IR_KNOB_PRESSED:
                switch( display_info.cur_screen ) {
                    case EMPEG_SCREEN_SET_RATING_BIAS:
                        database_info.stats_loaded = FALSE;
                        start_song_picker();
                        squash_signal( database_info.stats_finished );
                        break;
                    default:
                        break;
                }
                done = TRUE;
                display_info.in_menu = TRUE;
                display_info.menu_selection = menu_list[ display_info.cur_screen ].default_item;
                break;
        }
    }

    /* If we aren't in a menu, handle each screen's key events */
    if( !done ) {
        switch( display_info.cur_screen ) {
            case EMPEG_SCREEN_PLAY:
                switch( button ) {
                    case IR_TOP_BUTTON_PRESSED:
                        do_toggle_player_command();
                        break;
                    case IR_LEFT_BUTTON_PRESSED:
                        do_set_player_command( CMD_STOP );
                        break;
                    case IR_RIGHT_BUTTON_PRESSED:
                        do_set_player_command( CMD_SKIP );
                        break;
                    case IR_KNOB_LEFT:
                        squash_lock( player_info.lock );
                        sound_adjust_volume( player_info.device, -2 );
                        squash_unlock( player_info.lock );
                        break;
                    case IR_KNOB_RIGHT:
                        squash_lock( player_info.lock );
                        sound_adjust_volume( player_info.device, +2 );
                        squash_unlock( player_info.lock );
                        break;
                    case IR_0:
                    case IR_1:
                    case IR_2:
                    case IR_3:
                    case IR_4:
                    case IR_5:
                    case IR_6:
                    case IR_7:
                    case IR_8:
                    case IR_9:
                        do_song_rating_adjust( FALSE, button - IR_0 );
                        break;
                    case IR_DIRECT:
                        do_song_rating_adjust( FALSE, 10 );
                        break;
                    case IR_STAR:
                        do_song_rating_adjust( FALSE, -1 );
                        break;
                    default:
                        break;
                }
                break;
            case EMPEG_SCREEN_PLAYLIST:
                switch( button ) {
                    case IR_KNOB_LEFT:
                    case IR_LEFT_BUTTON_PRESSED:
                        squash_lock( song_queue.lock );
                        if( song_queue.selected > 0 ) {
                            song_queue.selected--;
                        }
                        squash_unlock( song_queue.lock );
                        break;
                    case IR_KNOB_RIGHT:
                    case IR_RIGHT_BUTTON_PRESSED:
                        squash_lock( song_queue.lock );
                        if( song_queue.selected < song_queue.size-1 ) {
                            song_queue.selected++;
                        }
                        squash_unlock( song_queue.lock );
                        break;
                    case IR_TOP_BUTTON_PRESSED:
                        do_remove_playlist_entry();
                        break;
                    default:
                        break;
                }
                break;
            case EMPEG_SCREEN_PLAY_RATE_SONG:
            case EMPEG_SCREEN_PLAYLIST_RATE_SONG:
                switch( button ) {
                    case IR_KNOB_LEFT:
                    case IR_LEFT_BUTTON_PRESSED:
                        do_song_rating_adjust( TRUE, -1 );
                        break;
                    case IR_KNOB_RIGHT:
                    case IR_RIGHT_BUTTON_PRESSED:
                        do_song_rating_adjust( TRUE, +1 );
                        break;
                    default:
                        break;
                }
                break;
            case EMPEG_SCREEN_SET_RATING_BIAS:
                switch( button ) {
                    case IR_KNOB_LEFT:
                    case IR_LEFT_BUTTON_PRESSED:
                        config.db_manual_rating_bias -= 0.1;
                        config.db_manual_rating_bias = (double)(int)(config.db_manual_rating_bias*10)/10;
                        if( config.db_manual_rating_bias < 0.0 ) {
                            config.db_manual_rating_bias = 0.0;
                        }
                        break;
                    case IR_KNOB_RIGHT:
                    case IR_RIGHT_BUTTON_PRESSED:
                        config.db_manual_rating_bias += 0.1;
                        config.db_manual_rating_bias = (double)(int)(config.db_manual_rating_bias*10)/10;
                        if( config.db_manual_rating_bias > 1.0 ) {
                            config.db_manual_rating_bias = 1.0;
                        }
                        break;
                    default:
                        break;
                }
                break;
            case EMPEG_SCREEN_SET_DISPLAY_BRIGHTNESS:
                switch( button ) {
                    case IR_KNOB_LEFT:
                    case IR_LEFT_BUTTON_PRESSED:
                        if( display_info.brightness >= 5 ) {
                            display_info.brightness -= 5;
                        }
                        set_display_brightness_empeg( display_info.brightness );
                        break;
                    case IR_KNOB_RIGHT:
                    case IR_RIGHT_BUTTON_PRESSED:
                        if( display_info.brightness <= 95 ) {
                            display_info.brightness += 5;
                        }
                        set_display_brightness_empeg( display_info.brightness );
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
    squash_broadcast( display_info.changed );
    squash_unlock( display_info.lock );
#endif
#endif
}

void *ir_monitor( void *input_data ) {
#ifdef EMPEG
    long last_button = -1, button = -1;
    int result, fd;

    fd = open("/dev/ir", O_RDONLY );
    if(fd == -1) {
        squash_error("Can't open ir");
    }

    while( 1 ) {
        fd_set read_fds;
        struct timeval select_timeval;
        struct timeval *select_timeval_pointer = NULL;
        bool button_held = FALSE;

        squash_log("ir loop");

        FD_ZERO( &read_fds );
        FD_SET( fd, &read_fds );

        select_timeval.tv_sec = 0;
        select_timeval.tv_usec = 500000;

        switch( last_button ) {
            case IR_TOP_BUTTON_PRESSED:
            case IR_RIGHT_BUTTON_PRESSED:
            case IR_LEFT_BUTTON_PRESSED:
            case IR_BOTTOM_BUTTON_PRESSED:
            case IR_KNOB_PRESSED:
                /*
                 * we are in a state where a button has been pressed, and that
                 * button may be held down.  so we only want to start a timeout
                 * of select_timeval seconds.  If we do not get a release event
                 * by then, that means the button is held down.
                 * to wait at most the select_timeval amount for a release event.
                 * if we timeout waiting for one, then that means we have a key
                 * held event (instead of a key press)
                 */
                select_timeval_pointer = &select_timeval;
                break;
            default:
                /*
                 * otherwise we aren't in a state where the button may be held down,
                 * then we can have our select wait indefinately.
                 */
                select_timeval_pointer = NULL;
                break;
        }

        button = -1;
        result = select( fd+1, &read_fds, NULL, NULL, select_timeval_pointer );
        if( result == 0 ) {
            /*
             * We timed out, we should only timeout if the last button was a holdable
             * button, but let's check just in case
             */
            if( select_timeval_pointer ) {
                button_held = TRUE;
                button = last_button;
                #ifdef INPUT_DEBUG
                squash_log( "Got (held) button %x", button );
                #endif
                last_button = -1;
            }
        } else if ( result < 0 ) {
            squash_error("Error calling select() in button loop");
        } else {
            result = read(fd, (char *) &button, 4);
            if( result == -1 ) {
                squash_error("Error reading buttons");
            }
            button_held = FALSE;
            #ifdef INPUT_DEBUG
            squash_log( "Got button %x", button );
            #endif
            switch( button ) {
                case IR_TOP_BUTTON_PRESSED:
                case IR_RIGHT_BUTTON_PRESSED:
                case IR_LEFT_BUTTON_PRESSED:
                case IR_BOTTOM_BUTTON_PRESSED:
                case IR_KNOB_PRESSED:
                    last_button = button;
                    button = -1;
                    break;
                case IR_TOP_BUTTON_RELEASED:
                case IR_RIGHT_BUTTON_RELEASED:
                case IR_LEFT_BUTTON_RELEASED:
                case IR_BOTTOM_BUTTON_RELEASED:
                case IR_KNOB_RELEASED:
                    if( last_button == (button & 0xFFFFFFFE) ) {
                        button = last_button;
                    } else {
                        button = -1;
                    }
                    last_button = -1;
                    break;
                default:
                    button = button;
                    last_button = -1;
            }
        }

        if( button != -1 ) {
            process_ir_event( button, button_held );
        }
    }
#endif
    return 0;
}

/*
 * Monitor keyboard events
 */
void *keyboard_monitor( void *input_data ) {
#ifndef NO_NCURSES
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
            case 'm':
                do_song_rating_adjust( TRUE, -1 );
                squash_broadcast( display_info.changed );
                break;
            case 'M':
                do_song_rating_adjust( TRUE, +1 );
                squash_broadcast( display_info.changed );
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

#endif
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
        squash_log("fifo loop");
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
            if( 0 ) {
#ifndef NO_NCURSES
            } else if( strncasecmp( buffer, "refresh\n", 8 ) == 0 ) {
                do_clear();
                do_resize();
            } else if ( strncasecmp( buffer, "spectrum_button\n", 16 ) == 0 ) {
                do_toggle_spectrum_state();
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
#endif
            } else if ( strncasecmp( buffer, "decrease_rating\n", 16 ) == 0 ) {
                do_song_rating_adjust( TRUE, -1 );
                squash_broadcast( display_info.changed );
            } else if ( strncasecmp( buffer, "increase_rating\n", 16 ) == 0 ) {
                do_song_rating_adjust( TRUE, +1 );
                squash_broadcast( display_info.changed );
            } else if ( strncasecmp( buffer, "quit\n", 5 ) == 0 ) {
                do_quit();
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
            }
        }
        fclose( fifo_info.fifo_file );
    }

    return 0;
}

#ifndef NO_NCURSES
/*
 * Force a clear of the screen
 */
void do_clear( void ) {
    squash_lock( display_info.lock );
    clear();
    refresh();
    squash_unlock( display_info.lock );
}
#endif

#ifndef NO_NCURSES
/*
 * Handle screen resize
 */
void do_resize( void ) {
    squash_lock( display_info.lock );
    draw_screen();
    squash_unlock( display_info.lock );
}
#endif

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

#ifndef NO_NCURSES
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
#endif

#ifndef NO_NCURSES
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
#endif

#ifndef NO_NCURSES
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
#endif

#ifndef NO_NCURSES
/*
 * Handle directional keys
 */
void do_arrow( enum direction_e direction ) {
    int delta;
    WINDOW *win;
    int xsize = 0, ysize = 0;
    int *selected = NULL, *max_size = NULL;

    squash_lock( display_info.lock );
    switch( display_info.focus ) {
        case WIN_NOW_PLAYING:
            break;
        case WIN_PLAYLIST:
            squash_lock( song_queue.lock );
            selected = &song_queue.selected;
            max_size = song_queue.wanted_size > song_queue.size ? &song_queue.wanted_size : &song_queue.size;
            break;
        case WIN_PASTLIST:
            squash_lock( past_queue.lock );
            selected = &past_queue.selected;
            max_size = &past_queue.size;
            break;
        default:
            break;
    }

    if( display_info.focus == WIN_PLAYLIST || display_info.focus == WIN_PASTLIST ) {

        if( (win = display_info.window[display_info.focus].window) != NULL ) {
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
                *selected = 0;
                break;
            case END:
                *selected = 0;
                delta = *max_size-1;
                break;
            default:
                delta = 0;
                break;
        }
        *selected = *selected + delta;

        if( *selected < 0 ) {
            *selected = 0;
        } else {
            if( *selected >= *max_size ) {
                *selected = *max_size - 1;
            }
        }
    }

    switch( display_info.focus ) {
        case WIN_NOW_PLAYING:
            break;
        case WIN_PLAYLIST:
            squash_unlock( song_queue.lock );
            break;
        case WIN_PASTLIST:
            squash_unlock( past_queue.lock );
            break;
        default:
            break;
    }

    squash_unlock( display_info.lock );
    squash_broadcast( display_info.changed );
}
#endif

#ifndef NO_NCURSES
/*
 * Handle delete event
 */
void do_delete( void ) {
    int i;
    song_queue_entry_t *queue_entry, *queue_last_entry, **head, **tail;
    int *selected, *size;

    squash_lock( display_info.lock );
    squash_wlock( database_info.lock );
    switch( display_info.focus ) {
        case WIN_PLAYLIST:
            squash_lock( song_queue.lock );
            selected = &song_queue.selected;
            size = &song_queue.size;
            head = &song_queue.head;
            tail = &song_queue.tail;
            queue_entry = song_queue.head;
            queue_last_entry = NULL;
            break;
        case WIN_PASTLIST:
            /* ignore deletes on past list */
            squash_wunlock( database_info.lock );
            squash_unlock( display_info.lock );
            return;
            /*
            squash_lock( past_queue.lock );
            selected = &past_queue.selected;
            size = &past_queue.size;
            head = &past_queue.head;
            tail = &past_queue.tail;
            queue_entry = past_queue.head;
            queue_last_entry = NULL;
            break;
            */
        default:
            squash_wunlock( database_info.lock );
            squash_unlock( display_info.lock );
            return;
    }

    if( *selected < *size && *selected >= 0 ) {
        for( i = 0; i < *selected; i++ ) {
            queue_last_entry = queue_entry;
            queue_entry = queue_entry->next;
        }
        if( queue_last_entry != NULL ) {
            queue_last_entry->next = queue_entry->next;
        }
        if( queue_entry == *head ) {
            *head = queue_entry->next;
        }
        if( queue_entry == *tail ) {
            *tail = queue_last_entry;
        }
        feedback( queue_entry->song_info , -1);
        squash_free( queue_entry );
        *size = *size - 1;
    }
    switch( display_info.focus ) {
        case WIN_PLAYLIST:
            squash_unlock( song_queue.lock );
            break;
        case WIN_PASTLIST:
            /* ignore deletes on past list */
            /*
            squash_unlock( past_queue.lock );
            */
            break;
        default:
            break;
    }
    squash_wunlock( database_info.lock );
    squash_unlock( display_info.lock );
    squash_broadcast( song_queue.not_full );
    squash_broadcast( display_info.changed );
}
#endif

#ifndef NO_NCURSES
/*
 * Handle Tab event
 */
void do_tab( void ) {
    squash_lock( display_info.lock );
    switch( display_info.focus ) {
        case WIN_NOW_PLAYING:
            display_info.focus = WIN_PLAYLIST;
            display_info.window[ WIN_PASTLIST ].state = WIN_STATE_HIDDEN;
            display_info.window[ WIN_PLAYLIST ].state = WIN_STATE_NORMAL;
            break;
        case WIN_PLAYLIST:
            display_info.focus = WIN_PASTLIST;
            display_info.window[ WIN_PASTLIST ].state = WIN_STATE_NORMAL;
            display_info.window[ WIN_PLAYLIST ].state = WIN_STATE_HIDDEN;
            break;
        case WIN_PASTLIST:
            display_info.focus = WIN_NOW_PLAYING;
            display_info.window[ WIN_PASTLIST ].state = WIN_STATE_HIDDEN;
            display_info.window[ WIN_PLAYLIST ].state = WIN_STATE_NORMAL;
            break;
        default:
            break;
    }
    draw_screen();
    squash_unlock( display_info.lock );
}
#endif

#ifndef NO_NCURSES
/*
 * Handle edit command
 */
void do_edit_command( void ) {
    char *editor;
    char *filename = NULL;
    int i;
    int pid;
    song_info_t *song = NULL;
    song_queue_entry_t *cur_entry;

    squash_rlock( database_info.lock );
    squash_lock( display_info.lock );
    switch( display_info.focus ) {
        case WIN_NOW_PLAYING:
            squash_lock( player_info.lock );
            song = player_info.song;
            squash_unlock( player_info.lock );
            break;
        case WIN_PLAYLIST:
            squash_lock( song_queue.lock );
            if( song_queue.selected < song_queue.size ) {
                cur_entry = song_queue.head;
                for( i = 0; i < song_queue.selected; i++ ) {
                    cur_entry = cur_entry->next;
                }
                song = cur_entry->song_info;
            }
            squash_unlock( song_queue.lock );
            break;
        case WIN_PASTLIST:
            squash_lock( past_queue.lock );
            if( past_queue.selected < past_queue.size ) {
                cur_entry = past_queue.head;
                for( i = 0; i < past_queue.selected; i++ ) {
                    cur_entry = cur_entry->next;
                }
                song = cur_entry->song_info;
            }
            squash_unlock( past_queue.lock );
            break;
        default:
            break;
    }

    if( song == NULL ) {
        squash_runlock( database_info.lock );
        squash_unlock( display_info.lock );
        return;
    }

    squash_asprintf(filename, "%s/%s.info", song->basename[ BASENAME_META ], song->filename);
    squash_runlock( database_info.lock );

    editor = getenv("EDITOR");
    if( editor == NULL ) {
        editor = "vi";
    }

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
    clear_song_meta( song );
    load_meta_data( song, TYPE_META );
    load_meta_data( song, TYPE_STAT );
    squash_wunlock( database_info.lock );

    display_init();
    draw_screen();
    squash_unlock( display_info.lock );
}
#endif
