/* aec.cpp
 *
 * Copyright (C) DFS Deutsche Flugsicherung (2004, 2005). 
 * All Rights Reserved.
 *
 * Acoustic Echo Cancellation NLMS-pw algorithm
 *
 * Version 0.3 filter created with www.dsptutor.freeuk.com
 * Version 0.3.1 Allow change of stability parameter delta
 * Version 0.4 Leaky Normalized LMS - pre whitening algorithm
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include "oss.h"
#include "aec.h"
#include "intercomd.h"

#include "tcp.h"

/* Vector Dot Product */
REAL dotp(REAL a[], REAL b[])
{
  REAL sum0 = 0.0, sum1 = 0.0;
  int j;

  for (j = 0; j < NLMS_LEN; j += 2) {
    // optimize: partial loop unrolling
    sum0 += a[j] * b[j];
    sum1 += a[j + 1] * b[j + 1];
  }
  return sum0 + sum1;
}


AEC::AEC()
{
  hangover = 0;
  memset(x, 0, sizeof(x));
  memset(xf, 0, sizeof(xf));
  memset(w, 0, sizeof(w));
  j = NLMS_EXT;
  delta = 0.0f;
  setambient(NoiseFloor);
  dfast = dslow = M75dB_PCM;
  xfast = xslow = M80dB_PCM;
  gain = 1.0f;
  Fx.init(2000.0f/RATE);
  Fe.init(2000.0f/RATE);
  
  aes_y2 = M0dB;
  
  fdwdisplay = -1;
  dumpcnt = 0;
  memset(ws, 0, sizeof(ws));
}

// Adrian soft decision DTD
// (Dual Average Near-End to Far-End signal Ratio DTD)
// This algorithm uses exponential smoothing with differnt
// ageing parameters to get fast and slow near-end and far-end
// signal averages. The ratio of NFRs term
// (dfast / xfast) / (dslow / xslow) is used to compute the stepsize
// A ratio value of 2.5 is mapped to stepsize 0, a ratio of 0 is
// mapped to 1.0 with a limited linear function.
inline float AEC::dtd(REAL d, REAL x)
{
  float stepsize;
  
  // fast near-end and far-end average
  dfast += ALPHAFAST * (fabsf(d) - dfast);
  xfast += ALPHAFAST * (fabsf(x) - xfast);
  
  // slow near-end and far-end average
  dslow += ALPHASLOW * (fabsf(d) - dslow);
  xslow += ALPHASLOW * (fabsf(x) - xslow);
  
  if (xfast < M70dB_PCM) {
    return 0.0;   // no Spk signal
  }
    
  if (dfast < M70dB_PCM) {
    return 0.0;   // no Mic signal
  }
  
  // ratio of NFRs
  float ratio = (dfast * xslow) / (dslow * xfast);
  
  // begrenzte lineare Kennlinie
  const float M = (STEPY2 - STEPY1) / (STEPX2 - STEPX1);
  if (ratio < STEPX1) {
    stepsize = STEPY1;
  } else if (ratio > STEPX2) {
    stepsize = STEPY2;
  } else {
    // Punktrichtungsform einer Geraden
    stepsize = M * (ratio - STEPX1) + STEPY1;
  }

  return stepsize;
}


inline void AEC::leaky()
// The xfast signal is used to charge the hangover timer to Thold.
// When hangover expires (no Spk signal for some time) the vector w
// is erased. This is my implementation of Leaky NLMS.
{
  if (xfast >= M70dB_PCM) {
    // vector w is valid for hangover Thold time
    hangover = Thold;
  } else {
    if (hangover > 1) {
      --hangover;
    } else if (1 == hangover) {
      --hangover;
      // My Leaky NLMS is to erase vector w when hangover expires
      // memset(w, 0, sizeof(w));
      hangover = 1;
    }
  }     
}


void AEC::openwdisplay() {
  // open TCP connection to program wdisplay.tcl
  // fdwdisplay = socket_async("127.0.0.1", 50999);
};

inline int satPCM(REAL e){
    // Saturation
  if (e > MAXPCM) {
    return MAXPCM;
  } else if (e < -MAXPCM) {
    return -MAXPCM;
  } else {
    return e;
  }
}

inline REAL AEC::nlms_pw(REAL d, REAL x_, float stepsize, int *py)
{
  x[j] = x_;
  xf[j] = Fx.highpass(x_);     // pre-whitening of x

  // calculate error value 
  // (mic signal - estimated mic signal from spk signal)
  REAL e = d;
  if (hangover > 0) {
    // e -= dotp(w, x + j);
    REAL yyy = dotp(w, x + j);
    if(py){
      *py = satPCM(yyy);
      // *py = x_;
    }
    e -= yyy;
  }
  REAL ef = Fe.highpass(e);     // pre-whitening of e
  
  // optimize: iterative dotp(xf, xf)
  dotp_xf_xf += (xf[j] * xf[j] - xf[j + NLMS_LEN - 1] * xf[j + NLMS_LEN - 1]);
  
  if (stepsize > 0.0) {
    // calculate variable step size
    REAL mikro_ef = stepsize * ef / dotp_xf_xf;
    
    // update tap weights (filter learning)
    int i;
    for (i = 0; i < NLMS_LEN; i += 2) {
      // optimize: partial loop unrolling
      w[i] += mikro_ef * xf[i + j];
      w[i + 1] += mikro_ef * xf[i + j + 1];
    }
  }
	
  if (--j < 0) {
    // optimize: decrease number of memory copies
    j = NLMS_EXT;
    memmove(x + j + 1, x, (NLMS_LEN - 1) * sizeof(REAL));
    memmove(xf + j + 1, xf, (NLMS_LEN - 1) * sizeof(REAL));
  }
  
  // Saturation
  if (e > MAXPCM) {
    return MAXPCM;
  } else if (e < -MAXPCM) {
    return -MAXPCM;
  } else {
    return e;
  }
}


int AEC::doAEC(int d_, int x_, int update, int *py)
{
  REAL d = (REAL) d_;
  REAL x = (REAL) x_;

  // Mic Highpass Filter - to remove DC
  d = acMic.highpass(d);

  // Mic Highpass Filter - cut-off below 300Hz
  d = cutoff.highpass(d);
  
  // Amplify, for e.g. Soundcards with -6dB max. volume
  d *= gain;

  // Spk Highpass Filter - to remove DC
  x = acSpk.highpass(x);

  // Double Talk Detector
  float stepsize = dtd(d, x);
  if (!update){
    stepsize = 0.0;
  }
  // Leaky (ageing of vector w)
  leaky();
  
  // Acoustic Echo Cancellation
  d = nlms_pw(d, x, stepsize, py); 
  
  if (fdwdisplay >= 0) {
    if (++dumpcnt >= (WIDEB*8000/10)) {
      // wdisplay creates 10 dumps per seconds = large CPU load!
      dumpcnt = 0;
      write(fdwdisplay, ws, DUMP_LEN*sizeof(float));
      // we don't check return value. This is not production quality!!!
      memset(ws, 0, sizeof(ws));
    } else {
      int i;
      for (i = 0; i < DUMP_LEN; i += 2) {
        // optimize: partial loop unrolling
        ws[i] += w[i];
        ws[i + 1] += w[i + 1];
      }
    }
  }

  return (int) d;
}
