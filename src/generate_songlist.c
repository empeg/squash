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
 *generate_songlist.c
 */

/*
 * This is a stand-alone utility for squash users.  The purpose of this
 * utility is to generate a list of songs using the same algorithm that
 * squash uses.  The intended use of this list is for a portable device
 * that cannot contain the owner's entire collection.
 */

#include "global.h"
#include "stat.h"
#include "database.h"
#include <sys/time.h>   /* for gettimeofday() */

/* to satisfy database wanting to update the display */
void draw_info() {
}

/* Needs two arguments, the number of songs to generate and whether to
 * clear the repeat counters. */

int main( int argc, char *argv[] ) {
    song_info_t *song;
    int x, total;
    struct timeval cur_time;
    char clear_repeat_counters;

    if( argc < 2 || !(total = atoi(argv[1]) ) ) {
        fprintf(stderr, "You must specify how many songs to generate\n");
        return 1;
    }

    if( argc < 3 ) {
        clear_repeat_counters = FALSE;
    } else {
        clear_repeat_counters = atoi(argv[2]);
    }

    gettimeofday( &cur_time, NULL );
    srand( cur_time.tv_sec );

    fprintf(stderr, "Loading configuration...\n");
    init_config();

    fprintf(stderr, "Loading filenames...\n");
    load_db_filenames();

    fprintf(stderr, "Loading statistics...\n");
    load_all_meta_data( TYPE_STAT );

    for( x = 0; x < database_info.song_count; x++ ) {
        database_info.songs[ x ].stat.repeat_counter = 0;
    }

    fprintf(stderr, "Calculating statistics...\n");
    start_song_picker();

    fprintf(stderr, "Generating %d files...\n", total);
    for( x = 0; x < total; x++ ) {
        song = &database_info.songs[ pick_song() ];

        printf("%s/%s\n", song->basename[ BASENAME_SONG ], song->filename );
    }

    return 0;
}
