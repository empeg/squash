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
 * playlist_manager.c
 */

#include "global.h"
#include "database.h"   /* for save_song() */
#include "stat.h"       /* for pick_song() */
#include "playlist_manager.h"

/*
 * Function called for the playlist_manager thread
 * This routine ensures that there is always song_queue.wanted_size
 * entries on the playlist.  It uses stat.c's pick_song
 * to choose what songs to add.
 */
void *playlist_manager( void *input_data ) {
    struct timeval cur_time;

    gettimeofday( &cur_time, NULL );
    srand( cur_time.tv_sec );

    /* First wait for the system to be loaded (wanted_size to be set). */
    /* Acquire lock */
    squash_lock( song_queue.lock );

    /* While we have too many songs (in this case, wanted_size has not been set) */
    while( song_queue.size >= song_queue.wanted_size ) {
        squash_wait( song_queue.not_full, song_queue.lock );
    }

    /* Unlock the mutexs */
    squash_unlock( song_queue.lock );

    /* Now load the playlist from disk */
    load_playlist();

    /* Load songs */
    while( 1 ) {
        squash_log("playlist loop");
        /* Acquire lock */
        squash_wlock( database_info.lock );
        squash_lock( song_queue.lock );

        /* We only get in this loop if there are too many (or just enough) songs */
        while( song_queue.size >= song_queue.wanted_size ) {
            /* Saving here garuntees a save only if there has been a removal,
               or the playlist had an addition (and only on the last addition) */
            save_playlist();
            squash_wunlock( database_info.lock );
            squash_wait( song_queue.not_full, song_queue.lock );
            squash_unlock( song_queue.lock );
            squash_wlock( database_info.lock );
            squash_lock( song_queue.lock );
        }

        /* We only get here if we have things to add */

        /* Check to see if there are any songs */
        if( database_info.song_count <= 0 ) {
            squash_error("This shouldn't happen, database is empty!");
        }

        /* Now wait for the statistics data to be loaded */
        while( !database_info.stats_loaded ) {
            squash_wunlock( database_info.lock );
            squash_wait( database_info.stats_finished, song_queue.lock );
            squash_unlock( song_queue.lock );
            squash_wlock( database_info.lock );
            squash_lock( song_queue.lock );
        }

        playlist_queue_song( &database_info.songs[ pick_song() ] );

        /* Save the database statistics to disk if we are done adding songs */
        if( song_queue.size >= song_queue.wanted_size ) {
            /* Sync pick_song changes to disk */
            int i;
            for( i = 0; i < database_info.song_count; i++ ) {
                save_song( &database_info.songs[i] );
            }
        }

        /* Unlock the mutexs */
        squash_unlock( song_queue.lock );
        squash_wunlock( database_info.lock );

        /* Signal the player thread waiting for songs */
        squash_broadcast( song_queue.not_empty );
        squash_broadcast( display_info.changed );
    }

    return 0;
}

/*
 * Save the current playlist to disk
 */
void save_playlist( void ) {
    FILE *playlist_file;
    time_t current_time;
    song_queue_entry_t *cur_song_queue_entry;

    if( config.db_readonly ) {
        return;
    }

    /* Open the file to write out the playlist */
    if( (playlist_file = fopen(config.playlist_manager_playlist_path, "w")) == NULL ) {
        squash_error( "Can't open file \"%s\" for writing", config.playlist_manager_playlist_path );
    }

    /* Write out header */
    time( &current_time );
    fprintf( playlist_file, "# Saved Squash playlist\n" );
    fprintf( playlist_file, "# Auto-Generated on %s\n", ctime(&current_time) );

    /* Write out the playlist */
    for( cur_song_queue_entry = song_queue.head; cur_song_queue_entry != NULL; cur_song_queue_entry = cur_song_queue_entry->next ) {
        fprintf( playlist_file, "%s/%s\n", cur_song_queue_entry->song_info->basename[ BASENAME_SONG ], cur_song_queue_entry->song_info->filename );
    }

    /* Close the playlist file */
    fclose( playlist_file );
}

/*
 * Load the stored playlist from disk
 */
void load_playlist( void ) {
    FILE *playlist_file;
    struct stat file_info;
    char *file_data;
    char *cur_data;
    char *this_line;
    song_info_t *song;

    /* Open the file to write out the playlist */
    if( (playlist_file = fopen(config.playlist_manager_playlist_path, "r")) == NULL ) {
        squash_log("Couldn't open playlist file, probably didn't exist");
        return;
    }

    if( fstat(fileno(playlist_file), &file_info) ) {
        squash_log("Couldn't stat playlist file");
        fclose( playlist_file );
        return;
    }

    if( (file_data = (char *)mmap( NULL, file_info.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fileno(playlist_file), 0)) == MAP_FAILED ) {
        squash_log("Couldn't mmap playlist file");
        fclose( playlist_file );
        return;
    }

    cur_data = file_data;
    while( cur_data != NULL ) {
        /* Acquire locks */
        squash_wlock( database_info.lock );
        squash_lock( song_queue.lock );

        this_line = strsep( &cur_data, "\n" );
        /* if not a line, empty, or is a comment */
        if( !(this_line == NULL || strlen(this_line) == 0 || this_line[0] == '#') ) {
            squash_log("Going to try to load: %s", this_line);

            /* if the file probably exists */
            if( !stat( this_line, &file_info ) ) {
                /* if the file really exists */
                if( (song = find_song_by_filename( this_line )) != NULL ) {
                    playlist_queue_song( song );
                    /* Signal the player thread waiting for songs */
                    squash_broadcast( song_queue.not_empty );
                }
            }
        }

        /* Release the locks */
        squash_unlock( song_queue.lock );
        squash_wunlock( database_info.lock );
    }

    /* Close the playlist file */
    fclose( playlist_file );

}

/*
 * Add an entry to the playlist
 */
void playlist_queue_song( song_info_t *song ) {
    song_queue_entry_t *new_song_queue_entry;

    /* Load the info metadata if the song hasn't been loaded yet */
    if( song->meta_key_count == -1 ) {
        song->meta_key_count = 0;
        load_meta_data( song, TYPE_META );
    }

    /* Allocate space for a new queue entry */
    squash_malloc( new_song_queue_entry, sizeof(song_queue_entry_t) );

    /* Setup the queue entry */
    new_song_queue_entry->song_info = song;
    new_song_queue_entry->next = (song_queue_entry_t *)NULL;

    /* Add to the queue */
    if( song_queue.head == NULL )
        song_queue.head = new_song_queue_entry;
    if( song_queue.tail != NULL )
        song_queue.tail->next = new_song_queue_entry;
    song_queue.tail = new_song_queue_entry;
    song_queue.size++;
    squash_log("song_queue.size = %d", song_queue.size);
}
