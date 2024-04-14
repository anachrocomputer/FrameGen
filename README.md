# FrameGen #

This is an experimental program to generate a sequence of video frames resembling the game "Defender".
Apart from having a parallax-scrolling background of fields and clouds, that is.

I wrote it in 2011 as a first step towards a fully interactive program that could display directly on the PC's screen.
Another iteration used the VGAlib to draw directly to the VGA screen in 320x200 mode.
But this version generates numerous PPM format still images and then assembles them, along with a soundtrack, into a video file by using 'ffmpeg'.

## Compiling and Building ##

To compile this code, you'll need the usual 'build-essential' package:

`sudo apt install build-essential`

along with 'ffmpeg':

`sudo apt install ffmpeg`

Once those are installed, you can simply run 'make':

`make`

The Makefile compiles the C code, runs it,
and then invokes 'ffmpeg' to generate the video file from the many PPM images.

