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
 * spectrum.c
 */

#include "global.h"
#include "display.h" /* for draw_spectrum() */
#include "spectrum.h"

/*
 * Called whenever the window containing the spectrum analyzer
 * is resized.  This routine recalculates many of spectrum's internal
 * variables to accomodate this new height.
 * This routine is called from display.c in it's resize routine.
 */
void spectrum_resize(int height, int width) {
    long i;
    int new_length, last_bar;
    double max_spectrum, exp_width;

    /* Free existing data */
    squash_free( spectrum_info.bitmap );
    squash_free( spectrum_info.cap_heights );
    squash_free( spectrum_info.bar_heights );
    squash_free( spectrum_info.bar_widths );

    spectrum_info.width = width - 2;
    spectrum_info.height = height - 2;
    if( spectrum_info.height > 0 && spectrum_info.width > 0 ) {
        squash_malloc( spectrum_info.bitmap, spectrum_info.height * spectrum_info.width * sizeof(chtype) );
        for( i = 0; i < (long)spectrum_info.width * (long)spectrum_info.height; i++ ) {
            spectrum_info.bitmap[i] = ' ';
        }

        squash_calloc( spectrum_info.cap_heights, spectrum_info.width, sizeof(double) );
        squash_calloc( spectrum_info.bar_heights, spectrum_info.width, sizeof(double) );

        squash_malloc( spectrum_info.bar_widths, (spectrum_info.width / 3) * sizeof(int) );

        /* Divide by two to not count the imaginary half in these calculations */
        max_spectrum = (double)SPECTRUM_WINDOW_SIZE*SPECTRUM_KEEP_AMOUNT / 2;
        last_bar = 0;
        for( i = 0; i < spectrum_info.width / 3; i++ ) {
            exp_width = pow(max_spectrum, 1/SPECTRUM_EXP_SCALE) - pow(last_bar, 1/SPECTRUM_EXP_SCALE);
            /* note that we round down this next line */
            new_length = pow((double)i*exp_width / ( (double)(spectrum_info.width/3-i) ) + 1, SPECTRUM_EXP_SCALE) - .5;
            /* if we rounded down to zero */
            if( new_length == 0 ) {
                /* and there is still some space */
                if( last_bar < max_spectrum ) {
                    new_length = 1;
                /* maybe we can borrow from the clipped part too */
                } else if ( last_bar < SPECTRUM_WINDOW_SIZE / 2 ) {
                    new_length = 1;
                    max_spectrum++;
                }
            }
            spectrum_info.bar_widths[i] = new_length + last_bar;
            last_bar = spectrum_info.bar_widths[i];

            /* or we could do a linear scale:
            spectrum_info.bar_widths[i] = (double)(i+1) / ((double)width/3.0) * (double)max_spectrum;*/
        }
    }
}

/*
 * Main routine for the spectrum thread.
 * It's duty is to update the display 20 times a second (roughly)
 * with the spectrum date.  Whether or not there is any new data
 * everytime is not checked.
 * The monitor will also pause processing if deactivated (via
 * spectrum_info.active = FALSE;).  This is done from the input.c
 * file.
 *
 * TODO: Perhaps an optimization to not call draw spectrum unless
 * new data has entered the ring buffer should be added. (But only
 * the half that does the fourier transform.  The renderer should
 * still be called so that you still have the fall-off caps.
 */
void *spectrum_monitor( void *input_data ) {
    struct timespec sleep_time = { 0, 50000000 };
    while( 1 ) {
        /* Acquire lock */
        squash_lock( display_info.lock );
        squash_lock( spectrum_info.lock );
        squash_lock( spectrum_ring.lock );

        /* if bitmap is null, then we do not know the size of the screen,
           if cap_heights is null, then we haven't been initialized at all. */
        while( !spectrum_ring.active || spectrum_info.bitmap == NULL || spectrum_info.cap_heights == NULL ) {
            squash_unlock( spectrum_info.lock );
            squash_unlock( spectrum_ring.lock );
            squash_wait( spectrum_ring.changed, display_info.lock );
            squash_lock( spectrum_info.lock );
            squash_lock( spectrum_ring.lock );
        }

        spectrum_prepare_render();
        squash_unlock( spectrum_ring.lock );

        spectrum_render();
        draw_spectrum();

        squash_unlock( spectrum_info.lock );
        squash_unlock( display_info.lock );

        nanosleep(&sleep_time, NULL);
    }
    return (void *) NULL;
}

/*
 * Initialize the variables needed by spectrum.  Those that require
 * the height and width of the screen are handled in spectrum_resize().
 */
void spectrum_init( void ) {
    int i;

    /* ring buffer */
    squash_malloc( spectrum_ring.ring , SPECTRUM_RING_SIZE*4*sizeof(char) );

    /* ff input buffer */
    squash_malloc( spectrum_info.in , SPECTRUM_WINDOW_SIZE*sizeof(fftw_complex) );

    /* ff output buffer */
    squash_malloc( spectrum_info.out , SPECTRUM_WINDOW_SIZE*sizeof(fftw_complex) );

    squash_malloc( spectrum_info.window , SPECTRUM_WINDOW_SIZE*sizeof(double) );

    for( i = 0; i < SPECTRUM_WINDOW_SIZE; i++ ) {
        spectrum_info.window[i] = (1.0-cos(2.0*3.1415926/(double)SPECTRUM_WINDOW_SIZE*(double)i))/2.0;
    }

    spectrum_info.bitmap = NULL;
    spectrum_info.cap_heights = NULL;
    spectrum_info.bar_heights = NULL;

    spectrum_info.plan = fftw_create_plan(SPECTRUM_WINDOW_SIZE, FFTW_FORWARD, FFTW_ESTIMATE);

    spectrum_ring.active = FALSE;
    spectrum_ring.head = 0;
    spectrum_ring.tail = 0;

    for( i = 0; i < SPECTRUM_WINDOW_SIZE; i++ ) {
        spectrum_info.out[i].im = 0.0;
        spectrum_info.out[i].re = 0.0;
    }

    return;
}

/*
 * Yep, free up that memory.
 */
void spectrum_deinit( void ) {
    squash_free(spectrum_ring.ring);
    fftw_destroy_plan(spectrum_info.plan);
    squash_free(spectrum_info.out);
    squash_free(spectrum_info.in);
    squash_free(spectrum_info.bitmap);
    squash_free(spectrum_info.cap_heights);
    squash_free(spectrum_info.bar_heights);
    squash_free(spectrum_info.bar_widths);
}

/*
 * This clears the ring buffer and the current calculated
 * bar_heights (used to make the bitmap).  And the actual
 * bitmap.  The cap_heights are not reset, so that they are
 * able to fall off in an ultra-cool fashion.
 * Technically, the bitmap doesn't really need to be cleared.
 * (Since spectrum_handle_new_data() will be called right
 * before draw_spectrum() and the former is responsible for
 * filling up the bitmap).
 */
void spectrum_reset( sound_format_t sound_format ) {
    long i;

    /* Acquire lock */
    squash_lock( spectrum_ring.lock );
    squash_lock( spectrum_info.lock );

    spectrum_ring.head = 0;
    spectrum_ring.tail = 0;

    spectrum_info.sound_format = sound_format;

    /* Reset the spectrum buffer */
    for( i = 0; i < SPECTRUM_WINDOW_SIZE; i++ ) {
        spectrum_info.out[i].im = 0.0;
        spectrum_info.out[i].re = 0.0;
    }

    /* Reset the positions of the bar_widths */
    if( spectrum_info.bitmap != NULL ) {
        for( i = 0; i < (long)spectrum_info.width * (long)spectrum_info.height; i++ ) {
            spectrum_info.bitmap[i] = ' ';
        }
    }

    /* Reset the bar heights */
    if( spectrum_info.bar_heights != NULL ) {
        for( i = 0; i < spectrum_info.width; i++ ) {
            spectrum_info.bar_heights[i] = 0.0;
        }
    }

    /* Release lock */
    squash_unlock( spectrum_info.lock );
    squash_unlock( spectrum_ring.lock );
}

/*
 * this function will add data from the frame to the ring buffer -- possibly
 * overwriting a small amount of unprocessed data during the beginning.
 */
void spectrum_update( frame_data_t frame ) {
    /* lengths are in units of samples (4 bytes)
     * data->ring is an char array so 4 chars per sample
     * frame.pcm_data is assumed to also be a char array with 4 chars per sample
     */
    int to_add, first_add, second_add, cur_size;
    int i;

    /* Acquire lock */
    squash_lock( spectrum_ring.lock );

    to_add = frame.pcm_size / 4;

    /* see if our added data wraps */
    if( spectrum_ring.tail + to_add >= SPECTRUM_RING_SIZE ) {
        first_add = SPECTRUM_RING_SIZE - spectrum_ring.tail - 1;
        second_add = to_add - first_add;
        memcpy(&spectrum_ring.ring[spectrum_ring.tail*4], &frame.pcm_data[0], first_add*4);
        memcpy(&spectrum_ring.ring[0], &frame.pcm_data[first_add*4], second_add*4);
    } else {
        /* data does not wrap */
        memcpy(&spectrum_ring.ring[spectrum_ring.tail*4], &frame.pcm_data[0], to_add*4);
    }

    i = (spectrum_ring.tail + to_add ) % SPECTRUM_RING_SIZE;  /* where tail will be */
    if( spectrum_ring.head == (spectrum_ring.tail + 1) % SPECTRUM_RING_SIZE ) {
        /* already full */
        spectrum_ring.tail = i;
        spectrum_ring.head = (i + 1) % SPECTRUM_RING_SIZE;
    } else {
        /* not already full */
        cur_size = ( spectrum_ring.tail - spectrum_ring.head + SPECTRUM_RING_SIZE ) % SPECTRUM_RING_SIZE;
        if( cur_size + to_add >= SPECTRUM_RING_SIZE ) {
            /* will be full after setting data->tail */
            spectrum_ring.tail = i;
            spectrum_ring.head = (i + 1) % SPECTRUM_RING_SIZE;
        } else {
            /* will not be full after setting data->tail */
            spectrum_ring.tail = i;
        }
    }

    /* Release lock */
    squash_unlock( spectrum_ring.lock );
    squash_broadcast( spectrum_ring.changed );
}

/*
 * This routine copies and conditions the ring buffer into the
 * fftw in buffer
 */
void spectrum_prepare_render( void ) {
    short sample;
    double window_factor;
    int index, i;

    if( (spectrum_ring.tail + 1) % SPECTRUM_RING_SIZE == spectrum_ring.head ) {
        index = spectrum_ring.head;
        for(i = 0; i < SPECTRUM_WINDOW_SIZE; i++ ) {
            window_factor = spectrum_info.window[i];
            sample = ((short *)spectrum_ring.ring)[index];
            spectrum_info.in[i].re = (double)(sample)*window_factor/32768.0;
            index = (index + 1) % SPECTRUM_RING_SIZE;
            if (spectrum_info.sound_format.channels == 2) {
                sample = ((short *)spectrum_ring.ring)[index];
                spectrum_info.in[i].im = (double)(sample)*window_factor/32768.0;
                index = (index + 1) % SPECTRUM_RING_SIZE;
            } else {
                spectrum_info.in[i].im = 0.0;
            }
        }
    }
}

/*
 * This routine does the processing on the fftw in buffer into
 * output spectrum data.  It also renders the bitmap for draw_spectrum().
 * draw_spectrum() will do the actual drawing.
 */
void spectrum_render( void ) {
    chtype *cur_bitmap;
    int height, width;
    int i, j, x, cur_height, y, max_spectrum, power_count;
    double avg_power;

    fftw_one(spectrum_info.plan, spectrum_info.in, spectrum_info.out);

    height = spectrum_info.height;
    width = spectrum_info.width;

    max_spectrum = SPECTRUM_WINDOW_SIZE / 2;
    i = 0;
    j = SPECTRUM_WINDOW_SIZE - 1;
    /* for each bar */
    for( x = 0; x < width / 3; x++ ) {
        /* Fill the bar up, find the avg_power */
        avg_power = 0;
        power_count = i;
        while( i < spectrum_info.bar_widths[x]) {
            avg_power += fabs(spectrum_info.out[i].re * spectrum_info.out[i].re - \
                                                spectrum_info.out[i].im * spectrum_info.out[i].im );
            avg_power += fabs(spectrum_info.out[j].re * spectrum_info.out[j].re - \
                                                spectrum_info.out[j].im * spectrum_info.out[j].im );
            j--;
            i++;
        }
        power_count = i - power_count;
        avg_power /= (double)power_count * 2 * (double)SPECTRUM_WINDOW_SIZE;
        avg_power = 10 * log10( avg_power );

        /* Calculate actual bar height */
        cur_height = ( avg_power - SPECTRUM_MIN_DB )/(SPECTRUM_MAX_DB - SPECTRUM_MIN_DB)*(double)height;

        if( cur_height > spectrum_info.bar_heights[x] ) {
            cur_height = (spectrum_info.bar_heights[x] * SPECTRUM_BAR_RISE_OLD + cur_height * SPECTRUM_BAR_RISE_NEW) /
                (SPECTRUM_BAR_RISE_OLD + SPECTRUM_BAR_RISE_NEW);
        } else {
            cur_height = (spectrum_info.bar_heights[x] * SPECTRUM_BAR_FALL_OLD + cur_height * SPECTRUM_BAR_FALL_NEW) /
                (SPECTRUM_BAR_FALL_OLD + SPECTRUM_BAR_FALL_NEW);
        }
        spectrum_info.bar_heights[x] = cur_height;

        /* clipping isn't 100% necessary here,
         * but its good to be nice to your numbers */
        if( cur_height > height ) {
            cur_height = height;
        } else if( cur_height < 0 ) {
            cur_height = 0;
        }

        /* Calculate new bar falloff cap */
        spectrum_info.cap_heights[x] -= (double)height * SPECTRUM_BAR_CAP_FALL_RATE;
        if( spectrum_info.cap_heights[x] < cur_height ) {
            spectrum_info.cap_heights[x] = cur_height;
        }

        /* Draw bar_widths and caps */
        for( y = 0; y < height; y++ ) {
            cur_bitmap = &spectrum_info.bitmap[(long)y * width + (long)x * 3L];
            if( y < cur_height ) {
                *cur_bitmap++ = SPECTRUM_BAR_FILL_LEFT;
                *cur_bitmap++ = SPECTRUM_BAR_FILL_RIGHT;
            } else if ( y == (int)spectrum_info.cap_heights[x] ) {
                *cur_bitmap++ = SPECTRUM_BAR_CAP_LEFT;
                *cur_bitmap++ = SPECTRUM_BAR_CAP_RIGHT;
            } else {
                *cur_bitmap++ = ' ';
                *cur_bitmap++ = ' ';
            }
            *cur_bitmap++ = ' ';
        }
    }

    for( y = 0; y < height; y++ ) {
        cur_bitmap = &spectrum_info.bitmap[(long)y * width + (long)x * 3L];
        for( i = 0; i < width % 3; i++ ) {
            *cur_bitmap++ = ' ';
        }
    }
}

