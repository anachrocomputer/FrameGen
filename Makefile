
CC=gcc
LD=gcc

all: vid.mp4

vid.mp4: frame000.ppm
	ffmpeg -i frame%03d.ppm -i audio.wav -ac 2 -ab 192k -b 2000k -y vid.mp4

frame000.ppm: framegen
	./framegen

framgen.o: framegen.c
	$(CC) -c framegen.c

framegen: framegen.o
	$(LD) -o framegen framegen.o -lm
