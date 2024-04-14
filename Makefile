# Makefile for 'framegen'

CC=gcc
LD=gcc
FFMPEG=ffmpeg

all: vid.mp4
.PHONY: all

vid.mp4: frame000.ppm
	$(FFMPEG) -i frame%03d.ppm -i audio.wav -ac 2 -ab 192k -b 2000k -y vid.mp4

frame000.ppm: framegen
	./framegen

framgen.o: framegen.c
	$(CC) -c framegen.c

framegen: framegen.o
	$(LD) -o framegen framegen.o -lm

# 'make clean' will delete all frame images, the audio file, and the object file
# It does not delete the final video file or the executable 'framegen'
clean:
	-rm -f frame*.ppm audio.wav framegen.o

.PHONY: clean

