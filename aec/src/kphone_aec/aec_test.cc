/* aec_test.cpp
 *
 * Copyright (C) DFS Deutsche Flugsicherung (2004). All Rights Reserved.
 *
 * Test stub for Acoustic Echo Cancellation NLMS-pw algorithm
 * Author: Andre Adrian, DFS Deutsche Flugsicherung
 * <Andre.Adrian@dfs.de>
 *
 * fortune says:
 * It's never as good as it feels, and it's never as bad as it seems.
 *
 * compile
c++ -DWIDEB=2 -O2 -o aec_test aec_test.cpp aec.cpp tcp.cpp -lm
 *
 * Version 1.3 set/get ambient in dB
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "tcp.h"
#include "aec.h"

#define TAPS  	(40*WIDEB*8)

typedef signed short MONO;

typedef struct {
  signed short l;
  signed short r;
} STEREO;

float dB2q(float dB)
{
  /* Dezibel to Ratio */
  return powf(10.0f, dB / 20.0f);
}

float q2dB(float q)
{
  /* Ratio to Dezibel */
  return 20.0f * log10f(q);
}


/* Read a raw audio file (8KHz sample frequency, 16bit PCM, stereo)
 * from stdin, echo cancel it and write it to stdout
 */
int main(int argc, char *argv[])
{
  STEREO inbuf[TAPS], outbuf[TAPS];
  float visualize;

  fprintf(stderr, "usage: aec_test [ambient in dB] <in.raw >out.raw\n");

  AEC aec;

  if (argc >= 2) {
    aec.setambient(MAXPCM*dB2q(atof(argv[1])));    
  }

  int taps;
  float ambient;
  while (taps = fread(inbuf, sizeof(STEREO), TAPS, stdin)) {
    int i;
    for (i = 0; i < taps; ++i) {
      int s0 = inbuf[i].l;      /* left channel microphone */
      int s1 = inbuf[i].r;      /* right channel speaker */

      /* and do NLMS */
      s0 = aec.doAEC(s0, s1);
      
      /* output one internal variable */
      // visualize = 16 * aec.hangover;
      visualize = 32000 * aec.stepsize;
      
      outbuf[i].l = (short)(visualize); /* left channel */
      outbuf[i].r = s0;         /* right channel echo cancelled mic */
    }

    fwrite(outbuf, sizeof(STEREO), taps, stdout);
  }
  ambient = aec.getambient();
  float ambientdB = q2dB(ambient / 32767.0f);
  fprintf(stderr, "Ambient = %2.0f dB\n", ambientdB);
  fflush(NULL);

  return 0;
}
