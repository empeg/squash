#
# squash Makefile
#
# NOTE: This file requires the GNU version of
#       make (gmake).
#

# Complier Flags
CC		:= gcc
CFLAGS	:= -O3 -pedantic -Wall
INCLUDE	:= -Iinclude
LDFLAGS	:= -lFLAC -lmad -lpthread -lm

ifdef EMPEG
#	CFLAGS := -DADVENTURE $(CFLAGS)

# Use this if you are using the debian tool chain
#	CC := arm-linux-gcc
#	INCLUDE	:=	$(INCLUDE) -I/usr/local/i686-linux-gnu/include -I/usr/arm-linux/include
#	LDFLAGS	:=	-L/usr/local/i686-linux-gnu/lib -L/usr/arm-linux/lib $(LDFLAGS)
#	LDFLAGS	:=	-static $(LDFLAGS)

# Use this if you are using the hijack tool chain
	CC := arm-empeg-linux-gcc
	INCLUDE	:=	$(INCLUDE) -I/usr/local/i686-arm-cross/include -I/usr/local/armtools-empeg/include
	LDFLAGS	:=	-L/usr/local/i686-arm-cross/lib -L/usr/local/armtools-empeg/lib $(LDFLAGS)

	NO_NCURSES := 1
	EMPEG_DSP := 1
	NO_FFTW := 1
	NO_ID3LIB := 1
	TREMOR := 1
	CFLAGS := -DEMPEG $(CFLAGS)
else
	CFLAGS := --std=gnu99
endif

ifdef USE_MAGIC
	CFLAGS := -DUSE_MAGIC $(CFLAGS)
else
endif

ifdef NO_NCURSES
	CFLAGS := -DNO_NCURSES $(CFLAGS)
else
	LDFLAGS := $(LDFLAGS) -lncurses
endif

ifdef EMPEG_DSP
	CFLAGS := -DEMPEG_DSP $(CFLAGS)
else
	LDFLAGS := $(LDFLAGS) -lao -ldl
endif

ifdef NO_FFTW
	CFLAGS := -DNO_FFTW $(CFLAGS)
else
	LDFLAGS := $(LDFLAGS) -lfftw
endif

ifdef NO_VORBIS_COMMENT
	CFLAGS := -DNO_VORBIS_COMMENT $(CFLAGS)
else
endif

ifdef NO_ID3LIB
	CFLAGS := -DNO_ID3LIB $(CFLAGS)
else
	LDFLAGS := $(LDFLAGS) -lz -lid3 -lstdc++
endif

ifdef TREMOR
	CFLAGS := -DTREMOR $(CFLAGS)
	LDFLAGS := $(LDFLAGS) -lvorbisidec
else
	LDFLAGS := $(LDFLAGS) -lvorbisfile -lvorbis
endif

ifdef LOCK_DEBUG
	DEBUG := 1
	CFLAGS := -DLOCK_DEBUG $(CFLAGS)
endif

ifdef INPUT_DEBUG
	DEBUG := 1
	CFLAGS := -DINPUT_DEBUG $(CFLAGS)
endif

ifdef DEBUG
	CFLAGS := -g -DDEBUG $(CFLAGS)
endif

#MAKEFLAGS := -j5

# Setup path information
vpath %.c src
vpath %.h include
vpath %.o obj

# Make sure we have an obj directory
$(shell umask 002 && mkdir -p obj)

# Rules
ifndef EMPEG
all: squash generate_songlist
else
all: squash empeg_poweroff
endif

SQUASH_OBJ_LIST := squash.o play_mp3.o play_ogg.o play_flac.o sound.o player.o playlist_manager.o database.o display.o spectrum.o global.o stat.o input.o global_squash.o
SQUASH_FILE_LIST := obj/player.o obj/playlist_manager.o obj/display.o obj/database.o obj/input.o obj/sound.o obj/play_flac.o obj/play_ogg.o obj/play_mp3.o obj/squash.o obj/spectrum.o obj/global.o obj/stat.o obj/global_squash.o
ifdef EMPEG
SQUASH_OBJ_LIST := $(SQUASH_OBJ_LIST) vfdlib.o
SQUASH_FILE_LIST := $(SQUASH_FILE_LIST) obj/vfdlib.o
endif

squash: $(SQUASH_OBJ_LIST)
	$(CC) -o squash $(SQUASH_FILE_LIST) $(LDFLAGS)

sound.o: %.o : %.c %.h global.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

play_flac.o play_ogg.o play_mp3.o: %.o : %.c %.h global.h database.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

playlist_manager.o: %.o : %.c %.h global.h database.h stat.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

spectrum.o: %.o : %.c %.h global.h display.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

player.o: %.o : %.c %.h global.h sound.h play_mp3.h play_ogg.h play_flac.h spectrum.h stat.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

display.o: %.o : %.c %.h global.h spectrum.h database.h stat.h version.h empeg/vfdlib.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

database.o: %.o : %.c %.h global.h player.h display.h play_ogg.h play_mp3.h play_flac.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

stat.o: %.o : %.c %.h global.h database.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

global.o: %.o : %.c %.h display.h database.h playlist_manager.h sound.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

global_squash.o: %.o : %.c %.h display.h database.h playlist_manager.h sound.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

input.o: %.o : %.c %.h global.h display.h player.h database.h sound.h stat.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

squash.o: %.o : %.c %.h global.h global_squash.h stat.h player.h playlist_manager.h database.h display.h input.h spectrum.h sound.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

vfdlib.o: empeg/vfdlib.h empeg/vfdlib.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/vfdlib.o empeg/vfdlib.c

empeg_poweroff: empeg_poweroff.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o empeg_poweroff src/empeg_poweroff.c

generate_songlist.o: %.o : %.c global.h database.h stat.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

generate_songlist: generate_songlist.o database.o global.o stat.o play_ogg.o play_mp3.o play_flac.o
	$(CC) $(LDFLAGS) -o generate_songlist obj/generate_songlist.o obj/database.o obj/global.o obj/stat.o obj/play_ogg.o obj/play_mp3.o obj/play_flac.o

clean:
	rm -rf squash* obj core empeg_poweroff generate_filelist

.PHONY: all clean
