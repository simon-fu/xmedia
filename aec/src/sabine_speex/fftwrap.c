#include "Platform.h"
#include "smallft.h"

#define MAX_FFT_SIZE 2048

void *spx_fft_init(int size)
{
    struct drft_lookup *table = (struct drft_lookup*)icm_alloc(1,sizeof(struct drft_lookup));
    spx_drft_init((struct drft_lookup *)table, size);
    return (void*)table;
}

void spx_fft_destroy(void *table)
{
    spx_drft_clear((struct drft_lookup*)table);
    icm_free(table);
}

void spx_fft(void *table,OsFloat *in,OsFloat *out)
{
    int i;

    OsFloat scale = 1.0/((struct drft_lookup *)table)->n;
    for (i = 0; i < ((struct drft_lookup *)table)->n; i++)
    {
        out[i] = scale*in[i];
    }
    //for (i = 0; i < ((struct drft_lookup *)table)->n; i++)
    //{
    //    out[i] = in[i];
    //}
    spx_drft_forward((struct drft_lookup *)table, out);
}

void spx_ifft(void *table,OsFloat *in,OsFloat *out)
{
    OsInt32 i;

    for (i = 0; i < ((struct drft_lookup *)table)->n; i++)
    {
        out[i] = in[i];
    }
    //OsFloat scale = 1.0f / ((struct drft_lookup *)table)->n;
    //for (i = 0; i < ((struct drft_lookup *)table)->n; i++)
    //{
    //    out[i] = scale*in[i];
    //}
    spx_drft_backward((struct drft_lookup *)table,out);
}