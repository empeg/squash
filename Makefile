#
# squash Makefile
#
# NOTE: This file requires the GNU version of
#       make (gmake).
#

# Complier Flags
CC		:= gcc
CFLAGS	:= -O3 -std=gnu99 -pedantic -Wall
INCLUDE	:= -Iinclude
LDFLAGS := -lvorbis -lao -ldl -lmad -lncurses -lfftw -lpthread -lm

ifdef TREMOR
	LDFLAGS := -lvorbisidec $(LDFLAGS)
else
	LDFLAGS := -lvorbisfile $(LDFLAGS)
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

MAKEFLAGS := -j5

# Setup path information
vpath %.c src
vpath %.h include
vpath %.o obj

# Make sure we have an obj directory
$(shell umask 002 && mkdir -p obj)

# Rules
all: squash

squash: squash.o play_mp3.o play_ogg.o sound.o player.o playlist_manager.o database.o display.o spectrum.o global.o stat.o input.o
	$(CC) $(LDFLAGS) -o squash obj/player.o obj/playlist_manager.o obj/display.o obj/database.o obj/input.o obj/sound.o obj/play_ogg.o obj/play_mp3.o obj/squash.o obj/spectrum.o obj/global.o obj/stat.o

sound.o: %.o : %.c %.h global.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

play_ogg.o play_mp3.o: %.o : %.c %.h global.h database.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

playlist_manager.o: %.o : %.c %.h global.h database.h stat.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

spectrum.o: %.o : %.c %.h global.h display.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

player.o: %.o : %.c %.h global.h sound.h play_mp3.h play_ogg.h spectrum.h stat.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

display.o: %.o : %.c %.h global.h spectrum.h database.h stat.h version.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

database.o: %.o : %.c %.h global.h stat.h player.h display.h play_ogg.h play_mp3.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

stat.o: %.o : %.c %.h global.h database.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

global.o: %.o : %.c %.h display.h database.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

input.o: %.o : %.c %.h global.h display.h player.h database.h stat.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

squash.o: %.o : %.c %.h global.h player.h playlist_manager.h database.h display.h input.h spectrum.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o obj/$*.o src/$*.c

clean:
	rm -rf squash* obj core

.PHONY: all clean
