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
 *stat.c
 */
#include "global.h"
#include "database.h" /* for save_song() */
#include "stat.h"

/*
 * Converts a song's statistics into a rating of goodness.
 * The value will be between -1 and 1 exclusive.
 */
double get_rating( stat_info_t stat ) {
    return (double)( stat.play_count - stat.skip_count ) /
           (double)( stat.play_count + stat.skip_count + 1 );
}

/*
 * Initializes the statistics of all songs that are needed
 * to calculate which song to pick next.  Currently this is
 * the average and the standard deviation.
 * If a song's rating changes (that is play_count or skip_count),
 * the sum and sqr_sum must be adjusted to reflect those changes.
 */
void start_song_picker() {
    int i;
    double sum = 0.0;
    double sqr_sum = 0.0;
    long play_sum = 0.0;
    long play_sqr_sum = 0.0;
    long skip_sum = 0.0;
    long skip_sqr_sum = 0.0;
    for( i = 0; i < database_info.song_count; i++ ) {
        double rating = get_rating( database_info.songs[i].stat );
        sum += rating;
        sqr_sum += rating * rating;
        play_sum += database_info.songs[i].stat.play_count;
        play_sqr_sum += database_info.songs[i].stat.play_count * database_info.songs[i].stat.play_count;
        skip_sum += database_info.songs[i].stat.skip_count;
        skip_sqr_sum += database_info.songs[i].stat.skip_count * database_info.songs[i].stat.skip_count;
    }
    database_info.sum = sum;
    database_info.sqr_sum = sqr_sum;
    database_info.play_sum = play_sum;
    database_info.play_sqr_sum = play_sqr_sum;
    database_info.skip_sum = skip_sum;
    database_info.skip_sqr_sum = skip_sqr_sum;
    database_info.stats_loaded = TRUE;
}

/*
 * Does a normal test on X using A (average) and S (standard
 * deviation) to convert X to a Z value.
 */
bool normal_test( double x, double a, double s ) {
    double pdf[5] = {    0.0000,
                        0.3413,
                        0.4772,
                        0.4987,
                        0.5000 };
    double z;
    bool sign;
    int pdf_pos;
    double area;
    double rand_val;

    z = (x - a) / s;
    if( isnan(z) ) {
        return TRUE;
    }

    sign    = z < 0;
    z = fabs(z);

    pdf_pos = ceil(z);

    if( z == 0.0 ) {
        area = 0.0;
    } else if( pdf_pos > 4 ) {
        area = 0.5;
    } else {
        double ratio = (double)pdf_pos - z;
        area = pdf[pdf_pos-1] * ratio + pdf[pdf_pos] * (1 - ratio);
    }
    if( sign ) {
        area = 0.5 - area;
    } else {
        area = 0.5 + area;
    }

    rand_val = (double)rand() / (RAND_MAX + 1.0);

    return area > rand_val;
}

/*
 * This routine picks a song on two criteria.
 * 1st: The songs rating (goodness) which is based on the play_count and
 * skip_count statistics that are gathered.  See get_rating().
 * 2nd: The repeat_counter.  The song must count down from 10 before it
 * is actually picked.  This helps avoid duplicate songs.
 */
unsigned int pick_song() {
    double avg;
    double std_dev;
    unsigned int canidate;
    double canidate_rating;

    /* This is a random pick.  It sucks.
    return (int)((double)database_info.song_count * rand() / (RAND_MAX + 1.0)); */

    avg = database_info.sum / database_info.song_count;
    std_dev = sqrt( fabs(database_info.sqr_sum / database_info.song_count - avg*avg) );

    while( 1 ) {
        canidate = (int)((double)database_info.song_count * rand() / (RAND_MAX + 1.0));
        canidate_rating = get_rating( database_info.songs[canidate].stat );

        if( !normal_test( canidate_rating, avg, std_dev ) ) {
            continue;
        }

        database_info.songs[canidate].stat.changed = TRUE;

        if( database_info.songs[canidate].stat.repeat_counter-- != 0 ) {
            continue;
        }

        database_info.songs[canidate].stat.repeat_counter = 10;
        return canidate;
    }
}

/*
 * When a song is skipped or played successfully, song feedback
 * should occur.  This routine alters these statistics and also
 * ensures that the global statistics are kept up to date.
 */
void feedback( song_info_t *song, short direction ) {
    double rating = get_rating( song->stat );

    database_info.sum -= rating;
    database_info.sqr_sum -= rating * rating;

    if( direction < 0 ) {
        database_info.skip_sqr_sum -= song->stat.skip_count * song->stat.skip_count;
        song->stat.skip_count++;
        database_info.skip_sum++;
        database_info.skip_sqr_sum += song->stat.skip_count * song->stat.skip_count;
    } else {
        database_info.play_sqr_sum -= song->stat.play_count * song->stat.play_count;
        song->stat.play_count++;
        database_info.play_sum++;
        database_info.play_sqr_sum += song->stat.play_count * song->stat.play_count;
    }
    song->stat.changed = 1;

    rating = get_rating( song->stat );
    database_info.sum += rating;
    database_info.sqr_sum += rating * rating;

    save_song( song );
}

