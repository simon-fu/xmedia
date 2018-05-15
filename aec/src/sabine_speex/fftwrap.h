#ifndef FFTWRAP_H
#define FFTWRAP_H

#include "Platform.h"

/** Compute tables for an FFT */
void *spx_fft_init(int size);

/** Destroy tables for an FFT */
void spx_fft_destroy(void *table);

/** Forward (real to half-complex) transform */
void spx_fft(void *table, OsFloat *in, OsFloat *out);

/** Backward (half-complex to real) transform */
void spx_ifft(void *table, OsFloat *in, OsFloat *out);

#endif
