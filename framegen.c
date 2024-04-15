/* framegen --- generate a sequence of video frames         2011-12-06 */
/* Copyright (c) 2011 John Honniball, Froods Software Development      */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

//#define SD

#define NFRAMES   (100)     /* Number of frames to generate */
#define FRAMERATE (25)      /* 25 f.p.s. video */
#define SAMPLERATE (44100)  /* CD audio sample rate */

#define MAXPATH  (256)
#define MAXLINE  (256)
#define MAXSAMPLES (SAMPLERATE / FRAMERATE)
#ifdef SD
#define MAXX     (720)
#define MAXY     (576)
#define MAXX_BG  (1024)
#else
#define MAXX     (224)
#define MAXY     (240)
#define MAXX_BG  (256)
#endif

/* Waveform samples for phase accumulators */
#define NSAMPLES (4096)
#define MAXVOICES (4)
#define MAXTONES  (4)

/* Voices for waveform synthesis */
#define VMUTE     (-1)
#define VSINE     (0)
#define VSQUARE   (1)
#define VTRIANGLE (2)
#define VSAWTOOTH (3)

#define MAXENVELOPES (6)
#define NENVSTEPS    (256)

#define ENVELOPE_LINEAR  (0)   /* Linear decay to zero */
#define ENVELOPE_EXP     (1)   /* Exponential decay, similar to a bell */
#define ENVELOPE_GATE    (2)   /* Simple on/off gating */
#define ENVELOPE_ADSR    (3)   /* Attack, decay, sustain, release */
#define ENVELOPE_SUSTAIN (4)   /* Short exponential attack and decay */
#define ENVELOPE_TREMOLO (5)   /* Like ADSR but with modulated sustain amplitude */

#define TWOPI (2.0 * M_PI)

/* Predefined colours */
#define WHITE   (0xff)
#define RED     (0xe0)
#define GREEN   (0x1c)
#define BLUE    (0x03)
#define MAGENTA (RED | BLUE)
#define BLACK   (0x00)

/* The frame buffer */
uint8_t Frame[MAXY][MAXX];
uint8_t Bgimg[MAXY][MAXX_BG];

#define MAXSPRITES (2)

static uint8_t Player[8][8] = {
   {     0,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,      0},
   { WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE},
   { WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE},
   { WHITE,  WHITE,  WHITE,   BLUE,   BLUE,  WHITE,  WHITE,  WHITE},
   { WHITE,  WHITE,  WHITE,   BLUE,   BLUE,  WHITE,  WHITE,  WHITE},
   { WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE},
   { WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE},
   {     0,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,      0}
};
static uint8_t Missile[6][7] = {
   {     0,  WHITE,  WHITE,  WHITE,  RED,  RED,    0},
   { WHITE,    RED,    RED,   BLUE,  RED,  RED,  RED},
   { WHITE,    RED,   BLUE,   BLUE, BLUE,  RED,  RED},
   { WHITE,    RED,   BLUE,   BLUE, BLUE,  RED,    1},
   { WHITE,    RED,    RED,   BLUE,  RED,  RED,    1},
   {     0,      1,      1,      1,    1,    1,    0}
};

static unsigned char Ship0[11][29];

/* The software sprites */
struct Sprite {
   int x0, y0;          /* Co-ords of top LH corner */
   int w, h;            /* Width and Height */       
   int visible;         /* Visibility flag */
   int transparent;     /* Colour for transparent pixels */
   unsigned char *tile; /* Pixels of the sprite (h * w) */
};

struct Sprite SpriteTab[MAXSPRITES];

/* Look-up tables for audio synthesis */
int16_t Wsample[MAXVOICES][NSAMPLES];
int16_t Wenvelope[MAXENVELOPES][NENVSTEPS];

#define LSAMPLES (25552 / 2)
int16_t Wlaser[LSAMPLES];

uint8_t Font[256][16] = {
#include "font2.h"
};

/* Tone generation by phase accumulator */
struct ToneRec {
   unsigned int PhaseAcc;
   unsigned int PhaseDelta;
   int PhaseDeltaDelta;
   char Voice;
   char Envelope;
   unsigned short int Ampl;
   unsigned short int Ampr;
   unsigned short int VolumeAcc;
   unsigned short int VolumeDelta;
   unsigned short int EnvAmp;
};

struct ToneRec ToneGen[MAXTONES];

/* Structure of WAV file headers */
struct WavHeader {
   char ChunkID[4];
   uint32_t ChunkSize;
   char Format[4];
};

struct FmtHeader {
   char SubChunkID[4];
   uint32_t SubChunkSize;
   uint16_t AudioFormat;
   uint16_t NumChannels;
   uint32_t SampleRate;
   uint32_t ByteRate;
   uint16_t BlockAlign;
   uint16_t BitsPerSample;
};

struct DataHeader {
   char SubChunkID[4];
   uint32_t SubChunkSize;
};

/* A single sample with left and right channels */
struct AudioSample {
   int16_t l;
   int16_t r;
};

/* The synthesised or sampled audio signal */
struct AudioSample Audio[MAXSAMPLES];
FILE *Wav;

int load_assets (void);
int load_background (void);
int load_sprites (void);
int load_sounds (void);
int generate_wavetables (void);
int open_audio (void);
void game_logic (int frame);
int clear_bg (int frame);
int draw_sprites (int frame);
int draw_overlay (int frame);
int render_audio (int frame);
int writeframe (int frame);
int writeaudio (int frame);
void export_image (const char *fname);
void close_audio (void);


int main (int argc, char *argv[])
{
   int frame;
   
   if (load_assets () != 0)
      exit (1);
   
   open_audio ();
   
   for (frame = 0; frame < NFRAMES; frame++) {
      game_logic (frame);
      
      clear_bg (frame);
      
      draw_sprites (frame);
      
      draw_overlay (frame);
      
      render_audio (frame);
      
      writeframe (frame);
      writeaudio (frame);
   }
   
   close_audio ();
   
   exit (0);
}


/* load_assets --- read in any game assets that are stored in files */

int load_assets (void)
{
   if (load_background () != 0)
      return (1);
   
   if (load_sounds () != 0)
      return (1);
      
   if (load_sprites () != 0)
      return (1);
      
   generate_wavetables ();
   
   return (0);
}


/* load_background --- load the scrolling background image from file */

int load_background (void)
{
   FILE *fp;
   char lin[MAXLINE];
   char fname[MAXPATH];
   int x, y;
   int pixel;
   int r, g, b;
   
   sprintf (fname, "bg%d.ppm", MAXY);
   
   if ((fp = fopen (fname, "r")) == NULL) {
      perror (fname);
      return (1);
   }
   
   fgets (lin, MAXLINE, fp);
   fgets (lin, MAXLINE, fp);
   fscanf (fp, "%d %d", &x, &y);
   fscanf (fp, "%d", &pixel);
   
   if ((x != MAXX_BG) || (y != MAXY) || (pixel != 255)) {
      fprintf (stderr, "%s: wrong image size or pixel depth\n", fname);
      fclose (fp);
      return (1);
   }
   
   for (y = 0; y < MAXY; y++) {
      for (x = 0; x < MAXX_BG; x++) {
         fscanf (fp, "%d %d %d", &r, &g, &b);
         r /= 32;
         g /= 32;
         b /= 64;
         Bgimg[y][x] = (r << 5) | (g << 2) | b;
      }
   }
   
   fclose (fp);
   
   return (0);
}


/* load_sprites --- read sprite images from files */

int load_sprites (void)
{
   FILE *fp;
   char lin[MAXLINE];
   char fname[MAXPATH];
   int x, y;
   int pixel;
   int r, g, b;
   
   sprintf (fname, "ship%d.ppm", 0);
   
   if ((fp = fopen (fname, "r")) == NULL) {
      perror (fname);
      return (1);
   }
   
   fgets (lin, MAXLINE, fp);
   fgets (lin, MAXLINE, fp);
   fscanf (fp, "%d %d", &x, &y);
   fscanf (fp, "%d", &pixel);
   
   if ((x != 29) || (y != 11) || (pixel != 255)) {
      fprintf (stderr, "%s: wrong image size or pixel depth\n", fname);
      fclose (fp);
      return (1);
   }
   
   for (y = 0; y < 11; y++) {
      for (x = 0; x < 29; x++) {
         fscanf (fp, "%d %d %d", &r, &g, &b);
         r /= 32;
         g /= 32;
         b /= 64;
         Ship0[y][x] = (r << 5) | (g << 2) | b;
      }
   }
   
   fclose (fp);
   
   return (0);
}


/* load_sounds --- read in audio samples */

int load_sounds (void)
{
   FILE *laser;
   int i;
   int16_t sample;
   
   if ((laser = fopen ("laser-02.raw", "r")) == NULL) {
      perror ("laser-02.raw");
      return (1);
   }
   
   for (i = 0; i < LSAMPLES; i++) {
      fread (&sample, sizeof (sample), 1, laser);
      Wlaser[i] = sample;
   }
   
   fclose (laser);
   
   return (0);
}


/* generate_wavetables --- generate sine, square, triangle waves */

int generate_wavetables (void)
{
   int i;
   double theta;
   double x;
   int16_t b;
   
   /* Generate lookup tables for audio waveforms */
   for (i = 0; i < NSAMPLES; i++) {
      /* Sine */
      theta = (TWOPI / NSAMPLES) * (double)i;
      b = (sin (theta) * 2047.0) + 0.5;
      Wsample[0][i] = b;

      /* Square */
      if (i < (NSAMPLES / 2))
         b = -2047;
      else
         b = 2047;

      Wsample[1][i] = b;
      
      /* Triangle */
      if (i == (NSAMPLES / 2)) {
         Wsample[2][i] = 2047;
      }
      else if (i < (NSAMPLES / 2)) {
         Wsample[2][i] = ((i * (2 * 4096)) / NSAMPLES) - 2047;
      }
      else {
         Wsample[2][i] = (((NSAMPLES - i) * (2 * 4096)) / NSAMPLES) - 2047; 
      }
      
      /* Sawtooth */
      Wsample[3][i] = ((i * 4096) / NSAMPLES) - 2047;

/*    sample[4][i] = (((double)rand () * 256.0) / (double)RAND_MAX) - 127; */
   }
   
   /* Generate lookup tables for audio envelopes */
   for (i = 0; i < NENVSTEPS; i++) {
      /* Linear */
      Wenvelope[ENVELOPE_LINEAR][i] = 255 - i;

      /* Exponential decay */
      x = (256 - i) / 32.0;
      b = exp2 (x) - 1.0;
      Wenvelope[ENVELOPE_EXP][i] = b;

      /* On/Off gate */
      if (i < (NSAMPLES - 1))
         Wenvelope[ENVELOPE_GATE][i] = 255;
      else
         Wenvelope[ENVELOPE_GATE][i] = 0;

      /* ADSR */
      if (i < 32) {
         x = (256 - i) / 32.0;
         b = exp2 (x) - 1.0;
      }
      else if (i < 192)
         b = 127;
      else {
         x = (256 - i) / (64.0 / 7.0);
         b = exp2 (x) - 1.0;
      }
      Wenvelope[ENVELOPE_ADSR][i] = b;

      /* Slow attack, sustain, decay */
      if (i < 32) {
         x = (32 - i) / 4.0;
         b = 256.0 - exp2 (x);
      }
      else if (i < 192)
         b = 255;
      else {
         x = (256 - i) / (64.0 / 8.0);
         b = exp2 (x) - 1.0;
      }
      Wenvelope[ENVELOPE_SUSTAIN][i] = b;

      /* Tremolo */
      if (i < 32) {
         x = (256 - i) / 32.0;
         b = exp2 (x) - 1.0;
      }
      else if (i < 192) {
         x = (i - 32) * 5.0 * TWOPI / 160.0;
         b = 127 + (64.0 * sin (x));
      }
      else {
         x = (256 - i) / (64.0 / 7.0);
         b = exp2 (x) - 1.0;
      }
      Wenvelope[ENVELOPE_TREMOLO][i] = b;
   }
   
   /* Initialise audio tone generators */
   for (i = 0; i < MAXTONES; i++) {
      ToneGen[i].Voice = VMUTE;
      ToneGen[i].Envelope = 0;
      ToneGen[i].PhaseAcc = 0;
      ToneGen[i].PhaseDelta = 0;
      ToneGen[i].PhaseDeltaDelta = 0;
      ToneGen[i].VolumeAcc = 0;
      ToneGen[i].VolumeDelta = 0;
      ToneGen[i].EnvAmp = 0;
      ToneGen[i].Ampl = 0;
      ToneGen[i].Ampr = 0;
   }

   return (0);
}


/* open_audio --- open the audio WAV file for writing */

int open_audio (void)
{
   struct WavHeader whdr;
   struct FmtHeader fhdr;
   struct DataHeader dhdr;

   if ((Wav = fopen ("audio.wav", "w")) == NULL) {
      fprintf (stderr, "can't open '%s' for writing\n", "audio.wav");
      exit (1);
   }
   
   /* Write WAV file header */
   whdr.ChunkID[0] = 'R';
   whdr.ChunkID[1] = 'I';
   whdr.ChunkID[2] = 'F';
   whdr.ChunkID[3] = 'F';
   whdr.ChunkSize = 36 + (MAXSAMPLES * 4 * NFRAMES);
   whdr.Format[0] = 'W';
   whdr.Format[1] = 'A';
   whdr.Format[2] = 'V';
   whdr.Format[3] = 'E';
   
   fwrite (&whdr, sizeof (whdr), 1, Wav);
   
   /* Write 'fmt' header */
   fhdr.SubChunkID[0] = 'f';
   fhdr.SubChunkID[1] = 'm';
   fhdr.SubChunkID[2] = 't';
   fhdr.SubChunkID[3] = ' ';
   fhdr.SubChunkSize = 16L;
   fhdr.AudioFormat = 1;
   fhdr.NumChannels = 2;
   fhdr.SampleRate = SAMPLERATE;
   fhdr.ByteRate = SAMPLERATE * 2 * 2;
   fhdr.BlockAlign = 2 * 2;
   fhdr.BitsPerSample = 16;
   
   fwrite (&fhdr, sizeof (fhdr), 1, Wav);

   /* Write 'data' header */
   dhdr.SubChunkID[0] = 'd';
   dhdr.SubChunkID[1] = 'a';
   dhdr.SubChunkID[2] = 't';
   dhdr.SubChunkID[3] = 'a';
   dhdr.SubChunkSize = MAXSAMPLES * 2 * 2 * NFRAMES;

   fwrite (&dhdr, sizeof (dhdr), 1, Wav);
}


/* game_logic --- execute all game logic for this frame */

void game_logic (int frame)
{
   struct Sprite *player = &SpriteTab[0];
   struct Sprite *missile = &SpriteTab[1];

   /* Initialise */
   if (frame == 0) {
      player->x0 = 16;
      player->y0 = frame + 25;
      player->h = 11;
      player->w = 29;
      player->visible = 1;
      player->transparent = MAGENTA;
      player->tile = (unsigned char *)Ship0;

      missile->x0 = 0;
      missile->y0 = 0;
      missile->w = 7;
      missile->h = 6;
      missile->visible = 0;
      missile->transparent = BLACK;
      missile->tile = (unsigned char *)Missile;
   }

   /* Move player sprite */
   player->x0 = 16;
   player->y0 = frame + 25;

   /* After one second, i.e. frame 25, fire! */
   if (frame == 25) {
      missile->x0 = player->x0 + 28;
      missile->y0 = player->y0 + 3;
      missile->visible = 1;

      ToneGen[0].Voice = VSINE;
      ToneGen[0].Envelope = ENVELOPE_TREMOLO;
      ToneGen[0].PhaseAcc = 0;
      ToneGen[0].PhaseDelta = (int)(((4294967295.0 * 440.0) / 44100.0) + 0.5);
      ToneGen[0].PhaseDeltaDelta = -200;
      ToneGen[0].VolumeAcc = 0;
      ToneGen[0].VolumeDelta = 300;
      ToneGen[0].EnvAmp = Wenvelope[ENVELOPE_TREMOLO][0];
      ToneGen[0].Ampl = 2;
      ToneGen[0].Ampr = 8;
   }

   /* Missile is active */
   if ((frame > 25) && (missile->x0 < MAXX) && (missile->y0 < MAXY)) {
      missile->x0 += 2;
      missile->y0 += 1;
   }

   /* After three seconds, i.e. frame 75, play note */
   if (frame == 75) {
      ToneGen[0].Voice = VSINE;
      ToneGen[0].Envelope = ENVELOPE_EXP;
      ToneGen[0].PhaseAcc = 0;
      ToneGen[0].PhaseDelta = (int)(((4294967295.0 * 261.0) / 44100.0) + 0.5);
      ToneGen[0].PhaseDeltaDelta = 0;
      ToneGen[0].VolumeAcc = 0;
      ToneGen[0].VolumeDelta = 500;
      ToneGen[0].EnvAmp = Wenvelope[ENVELOPE_EXP][0];
      ToneGen[0].Ampl = 2;
      ToneGen[0].Ampr = 8;
   }                     
}


/* clear_bg --- clear frame by drawing parallax-scrolling background */

int clear_bg (int frame)
{
   int x, y;
   int bx, by;
   
   for (y = 0; y < (MAXY / 4); y++) {
      for (x = 0; x < MAXX; x++) {
         bx = (x + frame) % MAXX_BG;
         by = y;
         Frame[by][x] = Bgimg[by][bx];
         
         bx = (x + (frame * 2)) % MAXX_BG;
         by = y + (MAXY / 4);
         Frame[by][x] = Bgimg[by][bx];

         bx = (x + (frame * 3)) % MAXX_BG;
         by = y + ((MAXY / 4) * 2);
         Frame[by][x] = Bgimg[by][bx];

         bx = (x + (frame * 4)) % MAXX_BG;
         by = y + ((MAXY / 4) * 3);
         Frame[by][x] = Bgimg[by][bx];
      }
   }
}


/* draw_sprites --- draw sprites by overwriting background */

int draw_sprites (int frame)
{
   int x, y;
   int s;
   int sx, sy;
   struct Sprite *sp;
   uint8_t *tp;
   
   /* Loop through sprites, drawing the visible ones */
   /* TODO: loop through in Z-order, from the rear */
   for (s = 0; s < MAXSPRITES; s++) {
      if (SpriteTab[s].visible) {
         sp = &SpriteTab[s];
         tp = sp->tile;
        
         for (y = sp->y0, sy = 0; sy < sp->h; y++, sy++) {
            for (x = sp->x0, sx = 0; sx < sp->w; x++, sx++) {
               if (*tp != sp->transparent) {
                  Frame[y][x] = *tp;
               }
               tp++;
            }
         }
      }
   }
}


/* draw_overlay --- overlay score on top of everything else */

int draw_overlay (int frame)
{
   int x, y;
   int x0, y0;
   int row, col;
   int i;
   char score[32];
   int len;
   uint8_t bitmask;
   
   sprintf (score, "%06d", frame);
   len = strlen (score);
   
   for (i = 0; i < len; i++) {
      x0 = (i * 8) + 8;
      y0 = MAXY - 18;
      
      for (y = y0, row = 0; row < 16; y++, row++) {
         for (x = x0, bitmask = 1, col = 0; col < 8; x++, bitmask <<= 1, col++) {
            if (Font[score[i]][row] & bitmask) {
               Frame[y][x] = WHITE;
            }
            else {
/*             Frame[y][x] = BLACK; */
            }
         }
      }
   }
}


/* render_audio --- generate audio samples for one frame of video */

int render_audio (int frame)
{
   int i;
   int t;
   struct ToneRec *tg;
   int ph;
   int en;
   static int l;
   static uint8_t volumeCounter = 0;
   
   for (i = 0; i < MAXSAMPLES; i++) {
      Audio[i].l = 0;
      Audio[i].r = 0;
      
      volumeCounter++;

      for (t = 0; t < MAXTONES; t++) {
         if (ToneGen[t].Voice != VMUTE) {
            tg = &ToneGen[t];
            ph = (tg->PhaseAcc & 0xfff00000) >> 20;
            tg->PhaseAcc += tg->PhaseDelta;
            tg->PhaseDelta += tg->PhaseDeltaDelta;
            Audio[i].l += (Wsample[tg->Voice][ph] * tg->EnvAmp) / 64;
            Audio[i].r += (Wsample[tg->Voice][ph] * tg->EnvAmp) / 16;
            
            if (volumeCounter == 0) {
               if (tg->VolumeAcc < (65535U - tg->VolumeDelta)) {
                  tg->VolumeAcc += tg->VolumeDelta;
                  en = tg->VolumeAcc >> 8;
                  tg->EnvAmp = Wenvelope[tg->Envelope][en];
               }
               else {
                  tg->Voice = VMUTE;
                  tg->EnvAmp = 0;
                  tg->VolumeAcc = 65535U;
               }
            }
         }
      }
   }
   
   if (frame == 25) {
      l = 0;
   }
   
   if ((frame >= 25) && (l < LSAMPLES)) {
      for (i = 0; (i < MAXSAMPLES) && (l < LSAMPLES); i++, l++) {
         Audio[i].l += Wlaser[l];
      }
   }
}


/* writeframe --- write a single frame of video to a file */

int writeframe (int frame)
{
   char fname[MAXPATH];
   
   sprintf (fname, "frame%03d.ppm", frame);
   
   export_image (fname);
}


/* export_image --- write the current video frame as PPM */

void export_image (const char *fname)
{
   struct Pixel {
      uint8_t r;
      uint8_t g;
      uint8_t b;
   };
   FILE *ppm;
   int x, y;
#ifndef PPM_ASCII
   uint8_t pixarray[MAXX][3];
#endif
   struct Pixel pel;
   
   if ((ppm = fopen (fname, "w")) == NULL) {
      fprintf (stderr, "can't open '%s' for writing\n", fname);
      exit (1);
   }

#ifdef PPM_ASCII
   fprintf (ppm, "P3\n");
#else
   fprintf (ppm, "P6\n");
#endif
   fprintf (ppm, "# Creator: framegen\n");
   fprintf (ppm, "%d %d\n", MAXX, MAXY);
   fprintf (ppm, "%d\n", 255);
   
   for (y = 0; y < MAXY; y++) {
      for (x = 0; x < MAXX; x++) {
         pel.r = (Frame[y][x] & 0xe0) >> 5;
         pel.g = (Frame[y][x] & 0x1c) >> 2;
         pel.b = (Frame[y][x] & 0x03);
         
         /* Scale RGB to 0-255 approx */
         pel.r *= 36;
         pel.g *= 36;
         pel.b *= 85;
         
#ifdef PPM_ASCII
         fprintf (ppm, "%d %d %d\n", pel.r, pel.g, pel.b);
#else               
         pixarray[x][0] = pel.r;
         pixarray[x][1] = pel.g;
         pixarray[x][2] = pel.b;
#endif
      }
#ifndef PPM_ASCII
      fwrite (pixarray, sizeof (pixarray), 1, ppm);
#endif
   }
   
   fclose (ppm);   
}


/* writeaudio --- write the current audio buffer as WAV */

int writeaudio (int frame)
{
   fwrite (Audio, sizeof (struct AudioSample), MAXSAMPLES, Wav);
}


/* close_audio --- close the audio WAV file */

void close_audio (void)
{
   fclose (Wav);
}

