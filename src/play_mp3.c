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
 * play_mp3.c
 */

#include "global.h"
#include "database.h" /* for insert_meta_data() */
#include "play_mp3.h"

/*
 * Open an mp3 file
 * sound_format will be modified and private state information
 * will be returned.
 */
void *mp3_open( char *filename, sound_format_t *sound_format ) {
    mp3_data_t *mp3_data;
    struct stat file_stat;
    struct mad_header m_header;

    /* Allocate space for data */
    squash_malloc( mp3_data, sizeof(mp3_data_t) );

    /* Open the song */
    if( (mp3_data->file = fopen(filename, "r")) == NULL ) {
        squash_error( "Unable to open file" );
    }

    /* Open the mp3 file */
    fstat(fileno(mp3_data->file), &file_stat);
    mp3_data->file_size = file_stat.st_size;
    mp3_data->buffer = mmap(0, mp3_data->file_size, PROT_READ, MAP_SHARED, fileno(mp3_data->file), 0);
    if( mp3_data->buffer == (void *)-1 ) {
        squash_error( "Unable to open MP3 file" );
    }

    mp3_data->pcm_data = NULL;
    mad_stream_init(&mp3_data->stream);
    mad_frame_init(&mp3_data->frame);
    mad_synth_init(&mp3_data->synth);
    mad_timer_reset(&mp3_data->timer);
    mad_stream_buffer(&mp3_data->stream, (unsigned char *)mp3_data->buffer, mp3_data->file_size);

    while( 1 ) {
        if( mad_header_decode(&m_header, &mp3_data->stream) == -1 ) {
            if( MAD_RECOVERABLE(mp3_data->stream.error) ) {
                continue;
            } else if( mp3_data->stream.error == MAD_ERROR_BUFLEN ) {
                squash_error("Unable to get mp3info (EOF)");
            } else {
                squash_error("Unable to get mp3info (Unrecoverable Error)");
            }
        }
        break;
    }

    sound_format->rate = m_header.samplerate;
    sound_format->channels = MAD_NCHANNELS(&m_header);
    sound_format->byte_format = SOUND_LITTLE;
    sound_format->bits = 16;

    mad_header_finish(&m_header);
    mad_stream_finish(&mp3_data->stream);

    mad_stream_init(&mp3_data->stream);
    mad_stream_buffer(&mp3_data->stream, (unsigned char *)mp3_data->buffer, mp3_data->file_size);

    /* Return data */
    return (void *)mp3_data;
}

#ifdef NO_ID3LIB
#else
/* All of this mess is needed for mp3_load_meta().  The mess will go away
 * once the id3lib C API is fleshed out some more.  Most of this is copied
 * from the source of id3lib with some additions, removals and modifications
 */
enum field_type_e { ID3FD_GeneralText, ID3FD_Text, ID3FD_UserText, ID3FD_URL, ID3FD_UserURL, ID3FD_SyncLyrics };

struct fid_to_db_s {
    int fid;
    char *short_name;
    char *long_name;
    bool foo;
    bool bar;
    enum field_type_e field_type;
    char *description;
    char *db_name;
};

/* copied and modified from id3lib/src/field.cpp because they don't have the C API complete.
 * Only uses the first and last field.  The others are from field.cpp. */
struct fid_to_db_s fid_to_db[] = {
  //                          short  long   tag    file
  // frame id                 id     id     discrd discrd field defs           description
  {ID3FID_COMMENT,           "COM", "COMM", false, false, ID3FD_GeneralText,   "Comments", "comment"},
  {ID3FID_SYNCEDLYRICS,      "SLT", "SYLT", false, false, ID3FD_SyncLyrics,    "Synchronized lyric/text", "lyrics_syncronized"},
  {ID3FID_ALBUM,             "TAL", "TALB", false, false, ID3FD_Text,          "Album/Movie/Show title", "album"},
  {ID3FID_BPM,               "TBP", "TBPM", false, false, ID3FD_Text,          "BPM (beats per minute)", "bpm"},
  {ID3FID_COMPOSER,          "TCM", "TCOM", false, false, ID3FD_Text,          "Composer", "artist_composer"},
  {ID3FID_CONTENTTYPE,       "TCO", "TCON", false, false, ID3FD_Text,          "Content type", "genre"},
  {ID3FID_DATE,              "TDA", "TDAT", false, false, ID3FD_Text,          "Date", ""},
  {ID3FID_LYRICIST,          "TXT", "TEXT", false, false, ID3FD_Text,          "Lyricist/Text writer", "artist_lyrics"},
  {ID3FID_FILETYPE,          "TFT", "TFLT", false, false, ID3FD_Text,          "File type", "file_type"},
  {ID3FID_CONTENTGROUP,      "TT1", "TIT1", false, false, ID3FD_Text,          "Title/songname/content description", "music_group"},
  {ID3FID_TITLE,             "TT2", "TIT2", false, false, ID3FD_Text,          "Title/songname/content description", "title"},
  {ID3FID_SUBTITLE,          "TT3", "TIT3", false, false, ID3FD_Text,          "Subtitle/Description refinement", "song_mix"},
  {ID3FID_LANGUAGE,          "TLA", "TLAN", false, false, ID3FD_Text,          "Language(s)", "language"},
  {ID3FID_SONGLEN,           "TLE", "TLEN", false, true,  ID3FD_Text,          "Length", "length"},
  {ID3FID_MEDIATYPE,         "TMT", "TMED", false, false, ID3FD_Text,          "Media type", "original_media"},
  {ID3FID_ORIGALBUM,         "TOT", "TOAL", false, false, ID3FD_Text,          "Original album/movie/show title", "album_original"},
  {ID3FID_ORIGLYRICIST,      "TOL", "TOLY", false, false, ID3FD_Text,          "Original lyricist(s)/text writer(s)", "artist_lyrics_original"},
  {ID3FID_ORIGARTIST,        "TOA", "TOPE", false, false, ID3FD_Text,          "Original artist(s)/performer(s)", "artist_original"},
  {ID3FID_LEADARTIST,        "TP1", "TPE1", false, false, ID3FD_Text,          "Lead performer(s)/Soloist(s)", "artist"},
  {ID3FID_BAND,              "TP2", "TPE2", false, false, ID3FD_Text,          "Band/orchestra/accompaniment", "artist_extra"},
  {ID3FID_CONDUCTOR,         "TP3", "TPE3", false, false, ID3FD_Text,          "Conductor/performer refinement", "artist_conductor"},
  {ID3FID_MIXARTIST,         "TP4", "TPE4", false, false, ID3FD_Text,          "Interpreted, remixed, or otherwise modified by", "artist_remix"},
  {ID3FID_PARTINSET,         "TPA", "TPOS", false, false, ID3FD_Text,          "Part of a set", "disc"},
  {ID3FID_TRACKNUM,          "TRK", "TRCK", false, false, ID3FD_Text,          "Track number/Position in set", "tracknumber"},
  {ID3FID_ISRC,              "TRC", "TSRC", false, false, ID3FD_Text,          "ISRC (international standard recording code)", "isrc"},
  {ID3FID_USERTEXT,          "TXX", "TXXX", false, false, ID3FD_UserText,      "User defined text information", "user_text"},
  {ID3FID_YEAR,              "TYE", "TYER", false, false, ID3FD_Text,          "Year", "year"},
  {ID3FID_UNSYNCEDLYRICS,    "ULT", "USLT", false, false, ID3FD_GeneralText,   "Unsynchronized lyric/text transcription", "lyrics"},
};
int fid_to_db_length = sizeof(fid_to_db) / sizeof(fid_to_db[0]);
#endif

/*
 * Set a song's metadata based on id3v2 tag,
 */
void mp3_load_meta( void *data, char *filename ) {
#ifdef NO_ID3LIB
#else
    ID3Tag *tag;
    ID3TagIterator *iterator;
    ID3Frame *frame;
    ID3Field *field;
    ID3_FrameID frame_id;

    int i, value_length;
    char *key, *value_raw, *cur_value, *cur_ptr;

    /* open the mp3 file and setup reading the id3 tag */
    tag = ID3Tag_New();
    ID3Tag_Link( tag, filename );
    iterator = ID3Tag_CreateIterator( tag );

    /* for each frame (artist, title, etc. */
    while( (frame = ID3TagIterator_GetNext( iterator )) != NULL ) {
        frame_id = ID3Frame_GetID( frame );

        /* find what frame_id represents */
        key = NULL;
        if( frame_id == ID3FID_NOFRAME || frame_id == ID3FID_PRIVATE ) {
            /* this is an unreadable frame by the id3lib library */
            continue;
        }
        for( i = 0; i < fid_to_db_length; i++ ) {
            if( fid_to_db[i].fid == frame_id ) {
                key = fid_to_db[i].db_name;
                break;
            }
        }
        if(key == NULL) {
            /* unknown frame, probably just one we can't do anything with
             * (like a picture)
             */
            continue;
        }

        /* get the text portion of this frame */
        field = ID3Frame_GetField( frame, ID3FN_TEXT );
        value_length = ID3Field_Size( field );

        squash_calloc( value_raw, value_length + 1, 1 );
        ID3Field_GetASCII( field, value_raw, value_length );

        /* now parse out any lines that might have gotten embedded */
        cur_ptr = value_raw;
        do {
            cur_value = cur_ptr;
            /* there -shouldn't- be anything other than \n but that doesn't mean
             * fools won't add them */
            cur_ptr = strpbrk( cur_value, "\n\r" );
            if( cur_ptr != NULL ) {
                while( cur_ptr - value_raw < value_length && (*cur_ptr == '\n' || *cur_ptr == '\r') ) {
                    *cur_ptr = '\0';
                    cur_ptr++;
                }
            }

            /* ok now add the key and value (need to make copies) */
            insert_meta_data( data, NULL, strdup(key), strdup( cur_value ) );
        } while ( cur_ptr != NULL && cur_ptr - value_raw < value_length );

        /* free up the data we got from the library */
        squash_free( value_raw );
    }

    /* free up the libraries data structures */
    ID3TagIterator_Delete( iterator );
    ID3Tag_Delete( tag );
#endif
}

/*
 * Decode a frame of mp3 data.
 */
frame_data_t mp3_decode_frame( void *data ) {
    mp3_data_t *mp3_data = (mp3_data_t *)data;
    frame_data_t frame_data;
    int i, x;
    int result;
    int channels;
    int pcm_size;
    unsigned int sample;

    /* Decode the next frame */
    result = mad_frame_decode(&mp3_data->frame, &mp3_data->stream);
    if (result != 0) {
        if (mp3_data->stream.error == MAD_ERROR_BUFLEN) {
            pcm_size = 0; /* EOF */
        } else if (MAD_RECOVERABLE(mp3_data->stream.error) ) {
            pcm_size = -1; /* Recoverable stream error */
        } else {
            pcm_size = -2; /* Unrecoverable stream error */
        }
    } else {
        mad_timer_add(&mp3_data->timer, mp3_data->frame.header.duration);
        mad_synth_frame(&mp3_data->synth, &mp3_data->frame);
        channels = MAD_NCHANNELS(&mp3_data->frame.header);
        pcm_size = mp3_data->synth.pcm.length*2*channels;
        squash_realloc( mp3_data->pcm_data, sizeof(char)*pcm_size );

        x = 0;
        for(i = 0; i < mp3_data->synth.pcm.length; i++) {
            sample = mad_to_16bit(mp3_data->synth.pcm.samples[0][i]);
            mp3_data->pcm_data[x++] = sample & 0xff;
            mp3_data->pcm_data[x++] = sample >> 8;

            if (channels > 1) {
                sample = mad_to_16bit(mp3_data->synth.pcm.samples[1][i]);
                mp3_data->pcm_data[x++] = sample & 0xff;
                mp3_data->pcm_data[x++] = sample >> 8;
            }
        }
    }
    frame_data.pcm_size = pcm_size;
    frame_data.position = mad_timer_count(mp3_data->timer, MAD_UNITS_MILLISECONDS);
    if( frame_data.position > mp3_data->duration ) {
        frame_data.position = mp3_data->duration;
    }

    /* Setup PCM buffer */
    if( frame_data.pcm_size <= 0 ) {
        frame_data.pcm_data = NULL;
    } else {
        frame_data.pcm_data = mp3_data->pcm_data;
    }

    /* Return the frame data */
    return frame_data;
}

/*
 * Seek to the seek_time position (in milliseconds) in
 * the opened song
 */
void mp3_seek( void *data, long seek_time ) {
    mp3_data_t *mp3_data = (mp3_data_t *)data;
    long new_file_position = (long)((double)mp3_data->file_size * seek_time / mp3_data->duration);
    mad_stream_buffer(&mp3_data->stream, (unsigned char *)&mp3_data->buffer[new_file_position], mp3_data->file_size - new_file_position);
    mad_timer_set( &mp3_data->timer, 0, seek_time, 1000 );
    mad_frame_mute( &mp3_data->frame );
    mad_synth_mute( &mp3_data->synth );
    mp3_data->stream.error = MAD_ERROR_NONE;
    mp3_data->stream.sync = 0;
    return;
}

/*
 * Close the opened song.
 */
void mp3_close( void *data ) {
    mp3_data_t *mp3_data = (mp3_data_t *)data;

    mad_synth_finish(&mp3_data->synth);
    mad_frame_finish(&mp3_data->frame);
    mad_stream_finish(&mp3_data->stream);

    if( munmap(mp3_data->buffer, mp3_data->file_size) ) {
        squash_error( "Unable to munmap" );
    }

    /* Close file */
    fclose( mp3_data->file );

    /* Free allocated storage */
    if ( mp3_data->pcm_data != NULL ) {
        free( mp3_data->pcm_data );
    }
    free( mp3_data );

    return;
}

/*
 * Takes the 24 bit sample given by libmad
 * and return a 16 bit sample.
 */
unsigned int mad_to_16bit(mad_fixed_t f) {
    f += (1L << (MAD_F_FRACBITS - 16));
    if (f >= MAD_F_ONE) {
        f = MAD_F_ONE - 1;
    } else if (f < -MAD_F_ONE) {
        f = -MAD_F_ONE;
    }
    return f >> (MAD_F_FRACBITS + 1 - 16);
}

/*
 * Return the number of milliseconds in the opened song
 */
long mp3_calc_duration(void * data) {
    mp3_data_t *mp3_data = (mp3_data_t *)data;
    struct mad_header m_header;

    while(1) {
        if( mad_header_decode(&m_header, &mp3_data->stream) == -1 ) {
            if( MAD_RECOVERABLE(mp3_data->stream.error) ) {
                continue;
            } else if( mp3_data->stream.error == MAD_ERROR_BUFLEN ) {
                break; /* EOF */
            } else {
                break; /* BAD ERROR, oh well */
            }
        }
        mad_timer_add(&mp3_data->timer, m_header.duration);
    }

    mp3_data->duration = mad_timer_count(mp3_data->timer, MAD_UNITS_MILLISECONDS);

    mad_header_finish(&m_header);
    mad_stream_finish(&mp3_data->stream);

    mad_stream_init(&mp3_data->stream);
    mad_timer_reset(&mp3_data->timer);
    mad_stream_buffer(&mp3_data->stream, (unsigned char *)mp3_data->buffer, mp3_data->file_size);

    return mp3_data->duration;
}
