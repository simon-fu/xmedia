/*
   The echo canceller is based on the MDF algorithm described in:

   J. S. Soo, K. K. Pang Multidelay block frequency adaptive filter, 
   IEEE Trans. Acoust. Speech Signal Process., Vol. ASSP-38, No. 2, 
   February 1990.
   
   We use the Alternatively Updated MDF (AUMDF) variant. Robustness to 
   double-talk is achieved using a variable learning rate as described in:
   
   Valin, J.-M., On Adjusting the Learning Rate in Frequency Domain Echo 
   Cancellation With Double-Talk. IEEE Transactions on Audio,
   Speech and Language Processing, Vol. 15, No. 3, pp. 1030-1034, 2007.
   http://people.xiph.org/~jm/papers/valin_taslp2006.pdf
   
   There is no explicit double-talk detection, but a continuous variation
   in the learning rate based on residual echo, double-talk and background
   noise.
*/
#include "speex_echo.h"
#include "fftwrap.h"

static const OsFloat MIN_LEAK       = 0.005f;
// Constants for the two-path filter
static const OsFloat VAR1_SMOOTH    = 0.36f;
static const OsFloat VAR2_SMOOTH    = 0.7225f;
static const OsFloat VAR1_UPDATE    = 0.5f;
static const OsFloat VAR2_UPDATE    = 0.25f;
static const OsFloat VAR_BACKTRACK  = 4.f;

/** Speex echo cancellation state. */
struct SpeexEchoState_
{
    OsInt32 frame_size; /**< Number of samples processed each time */
    OsInt32 window_size;
    OsInt32 M;
    OsInt32 cancel_count;
    OsInt32 adapted;
    OsInt32 saturated;
    OsInt32 screwed_up;
    OsInt32 C;          /** Number of input channels (microphones) */
    OsInt32 K;          /** Number of output channels (loudspeakers) */
    OsInt32 sampling_rate;
    OsFloat spec_average;
    OsFloat beta0;
    OsFloat beta_max;
    OsFloat sum_adapt;
    OsFloat leak_estimate;

    OsFloat *e;         /* scratch */
    OsFloat *x;         /* Far-end input buffer (2N) */
    OsFloat *X;         /* Far-end buffer (M+1 frames) in frequency domain */
    OsFloat *input;     /* scratch */
    OsFloat *y;         /* scratch */
    OsFloat *Y;         /* scratch */
    OsFloat *E;
    OsFloat *PHI;       /* scratch */
    OsFloat *W;         /* (Background) filter weights */

    OsFloat *foreground;/* Foreground filter weights */
    OsFloat Davg1;      /* 1st recursive average of the residual power difference */
    OsFloat Davg2;      /* 2nd recursive average of the residual power difference */
    OsFloat Dvar1;      /* Estimated variance of 1st estimator */
    OsFloat Dvar2;      /* Estimated variance of 2nd estimator */

    OsFloat *power;     /* Power of the far-end signal */
    OsFloat  *power_1;  /* Inverse power of far-end */
    OsFloat *wtmp;      /* scratch */
    OsFloat *Rf;        /* scratch */
    OsFloat *Yf;        /* scratch */
    OsFloat *Xf;        /* scratch */
    OsFloat *Eh;
    OsFloat *Yh;
    OsFloat   Pey;
    OsFloat   Pyy;
    OsFloat *window;
    OsFloat *prop;
    void *fft_table;
    OsFloat *memX, *memD, *memE;
    OsFloat preemph;
    OsFloat notch_radius;
    OsFloat *notch_mem;
};

static void filter_dc_notch16(const OsInt16 *in,OsFloat radius,OsFloat *out,OsInt32 len,OsFloat *mem,OsInt32 stride)
{
    OsInt32 i;
    OsFloat den2;
    den2 = radius*radius + 0.7*(1-radius)*(1-radius);
    // printf ("%d %d %d %d %d %d\n", num[0], num[1], num[2], den[0], den[1], den[2]);
    for (i = 0; i < len; i++)
    {
        OsFloat vin = in[i*stride];
        OsFloat vout = mem[0] + vin;
        mem[0] = mem[1] + 2*(-vin + radius*vout);
        mem[1] = vin - den2*vout;
        out[i] = radius*vout;
    }
}

// This inner product is slightly different from the codec version because of fixed-point
static OsFloat mdf_inner_prod(const OsFloat *x,const OsFloat *y,OsInt32 len)
{
    OsFloat sum=0;
    len >>= 1;
    while(len--)
    {
        OsFloat part = 0;
        part += (*x++)*(*y++);
        part += (*x++)*(*y++);
        sum += part;
    }
    return sum;
}

static void power_spectrum(const OsFloat *X,OsFloat *ps,OsInt32 N)
{
    OsInt32 i,j;
    ps[0] = X[0]*X[0];
    for (i=1,j=1; i < N-1; i+=2,j++)
    {
        ps[j] =  X[i]*X[i] + X[i+1]*X[i+1];
    }
    ps[j] = X[i]*X[i];
}

static void power_spectrum_accum(const OsFloat *X,OsFloat *ps, OsInt32 N)
{
    OsInt32 i,j;
    ps[0] += X[0]*X[0];
    for (i=1,j=1; i < N-1; i+=2,j++)
    {
        ps[j] += X[i]*X[i] + X[i+1]*X[i+1];
    }
    ps[j] += X[i]*X[i];
}

static void spectral_mul_accum(const OsFloat *X,const OsFloat *Y,OsFloat *acc,OsInt32 N,OsInt32 M)
{
   OsInt32 i,j;
   for (i = 0; i < N; i++)
      acc[i] = 0;
   for (j = 0; j < M; j++)
   {
      acc[0] += X[0]*Y[0];
      for (i = 1; i < N-1; i += 2)
      {
         acc[i] += (X[i]*Y[i] - X[i+1]*Y[i+1]);
         acc[i+1] += (X[i+1]*Y[i] + X[i]*Y[i+1]);
      }
      acc[i] += X[i]*Y[i];
      X += N;
      Y += N;
   }
}

// Compute weighted cross-power spectrum of a half-complex (packed) vector with conjugate
// w    st->power_1     回声占误差的比率
// p    st->prop[j]     P步长
// X    st->X
// E    st->E
// prod st->PHI
static void weighted_spectral_mul_conj(const OsFloat *w,const OsFloat p,const OsFloat *X,const OsFloat *E,OsFloat *prod,OsInt32 N)
{
    OsInt32 i,j;
    OsFloat mu;

    mu      = p * w[0];
    prod[0] = mu*X[0]*E[0];
    for (i=1,j=1; i < N-1; i+=2,j++)
    {
        mu          = p * w[j]; // P步长与最优步长相乘
        prod[i  ]   = mu*(( X[i  ]*E[i])+(X[i+1]*E[i+1]));
        prod[i+1]   = mu*((-X[i+1]*E[i])+(X[i  ]*E[i+1]));
    }
    mu = p * w[j];
    prod[i] = mu * (X[i]*E[i]);
}

// 计算P步长
static void mdf_adjust_prop(const OsFloat *W,OsInt32 N,OsInt32 M,OsInt32 P,OsFloat *prop)
{
    OsInt32 i, j, p;
    OsFloat max_sum = 1;
    OsFloat prop_sum = 1;
    for (i = 0; i < M; i++)
    {
        OsFloat tmp = 1;
        for (p = 0; p < P; p++)
        {
            for (j=0;j<N;j++)
            {
                tmp += W[p*N*M + i*N+j] * W[p*N*M + i*N+j];
            }
        }
        prop[i] = sqrt(tmp);
        if (prop[i] > max_sum)
            max_sum = prop[i];
    }
    for (i = 0; i < M; i++)
    {
        prop[i] += 0.1f*max_sum;
        prop_sum += prop[i];
    }
    for (i=0;i<M;i++)
    {
        prop[i] = (0.99f*prop[i])/prop_sum;
    }
}

SpeexEchoState *speex_echo_state_init(OsInt32 frame_size,OsInt32 filter_length)
{
   return speex_echo_state_init_mc(frame_size,filter_length,1,1);
}

SpeexEchoState *speex_echo_state_init_mc(OsInt32 frame_size,OsInt32 filter_length,OsInt32 nb_mic,OsInt32 nb_speakers)
{
    OsInt32 i,N,M, C, K;
    SpeexEchoState *st = (SpeexEchoState *)icm_alloc(1,sizeof(SpeexEchoState));

    st->K = nb_speakers;
    st->C = nb_mic;
    C=st->C;
    K=st->K;

    st->frame_size      = frame_size;
    st->window_size     = 2*frame_size;
    N = st->window_size;
    M = st->M = (filter_length+st->frame_size-1)/frame_size;
    st->cancel_count    = 0;
    st->sum_adapt       = 0;
    st->saturated       = 0;
    st->screwed_up      = 0;
    st->sampling_rate   = 8000; // This is the default sampling rate
    st->spec_average    = 1.0*st->frame_size/st->sampling_rate;
    st->beta0           = (2.0f*st->frame_size)/st->sampling_rate;
    st->beta_max        = (.5f*st->frame_size)/st->sampling_rate;
    st->leak_estimate   = 0;
    st->fft_table       = spx_fft_init(N);
    st->e               = (OsFloat*)icm_alloc(1,C*N*sizeof(OsFloat));
    st->x               = (OsFloat*)icm_alloc(1,K*N*sizeof(OsFloat));
    st->input           = (OsFloat*)icm_alloc(1,C*st->frame_size*sizeof(OsFloat));
    st->y               = (OsFloat*)icm_alloc(1,C*N*sizeof(OsFloat));
    st->Yf              = (OsFloat*)icm_alloc(1,(st->frame_size+1)*sizeof(OsFloat));
    st->Rf              = (OsFloat*)icm_alloc(1,(st->frame_size+1)*sizeof(OsFloat));
    st->Xf              = (OsFloat*)icm_alloc(1,(st->frame_size+1)*sizeof(OsFloat));
    st->Yh              = (OsFloat*)icm_alloc(1,(st->frame_size+1)*sizeof(OsFloat));
    st->Eh              = (OsFloat*)icm_alloc(1,(st->frame_size+1)*sizeof(OsFloat));
    st->X               = (OsFloat*)icm_alloc(1,K*(M+1)*N*sizeof(OsFloat));
    st->Y               = (OsFloat*)icm_alloc(1,C*N*sizeof(OsFloat));
    st->E               = (OsFloat*)icm_alloc(1,C*N*sizeof(OsFloat));
    st->W               = (OsFloat*)icm_alloc(1,C*K*M*N*sizeof(OsFloat));
    st->foreground      = (OsFloat*)icm_alloc(1,M*N*C*K*sizeof(OsFloat));
    st->PHI             = (OsFloat*)icm_alloc(1,N*sizeof(OsFloat));
    st->power           = (OsFloat*)icm_alloc(1,(frame_size+1)*sizeof(OsFloat));
    st->power_1         = (OsFloat*)icm_alloc(1,(frame_size+1)*sizeof(OsFloat));
    st->window          = (OsFloat*)icm_alloc(1,N*sizeof(OsFloat));
    st->prop            = (OsFloat*)icm_alloc(1,M*sizeof(OsFloat));
    st->wtmp            = (OsFloat*)icm_alloc(1,N*sizeof(OsFloat));

    for (i = 0; i < N; i++)
        st->window[i] = 0.5 - 0.5*cos(2*M_PI*i/N);

    for (i = 0; i <= st->frame_size; i++)
        st->power_1[i] = 1.0;
    for (i = 0; i < N*M*K*C; i++)
        st->W[i] = 0;

    {
        OsFloat sum = 0;
        // Ratio of ~10 between adaptation rate of first and last block
        OsFloat decay = exp(-2.4/M);
        st->prop[0] = 0.7;
        sum = st->prop[0];
        for (i = 1; i < M; i++)
        {
            st->prop[i] = st->prop[i-1] * decay;
            sum = sum + st->prop[i];
        }
        for (i = M-1; i >= 0; i--)
        {
            st->prop[i] = (0.8f * st->prop[i])/ sum;
        }
    }

    st->memX    = (OsFloat*)icm_alloc(1,K*sizeof(OsFloat));
    st->memD    = (OsFloat*)icm_alloc(1,C*sizeof(OsFloat));
    st->memE    = (OsFloat*)icm_alloc(1,C*sizeof(OsFloat));
    st->preemph = 0.9;

    if (st->sampling_rate < 12000)
        st->notch_radius = 0.9;
    else if (st->sampling_rate < 24000)
        st->notch_radius = 0.982;
    else
        st->notch_radius = 0.992;

    st->notch_mem           = (OsFloat*)icm_alloc(1,2*C*sizeof(OsFloat));
    st->adapted             = 0;
    st->Pey = st->Pyy       = 1.0;
    st->Davg1 = st->Davg2   = 0;
    st->Dvar1 = st->Dvar2   = 0.0;
    return st;
}

void speex_echo_state_reset(SpeexEchoState *st)
{
    OsInt32 i,M,N,C,K;
    st->cancel_count = 0;
    st->screwed_up = 0;
    N = st->window_size;
    M = st->M;
    C = st->C;
    K = st->K;
    for (i = 0; i < N*M; i++)
        st->W[i] = 0;

    for (i = 0; i < N*M; i++)
        st->foreground[i] = 0;

    for (i = 0; i < N*(M+1); i++)
        st->X[i] = 0;
    for (i = 0; i <= st->frame_size; i++)
    {
        st->power[i] = 0;
        st->power_1[i] = 1.0;
        st->Eh[i] = 0;
        st->Yh[i] = 0;
    }
    for (i = 0; i < N*C; i++)
    {
        st->E[i] = 0;
    }
    for (i = 0; i < N*K; i++)
    {
        st->x[i] = 0;
    }
    for (i = 0; i < 2*C; i++)
        st->notch_mem[i] = 0;
    for (i = 0; i < C; i++)
        st->memD[i] = st->memE[i] = 0;
    for (i = 0; i < K; i++)
        st->memX[i] = 0;

    st->saturated   = 0;
    st->adapted     = 0;
    st->sum_adapt   = 0;
    st->Pey = st->Pyy = 1.0;
    st->Davg1 = st->Davg2 = 0;
    st->Dvar1 = st->Dvar2 = 0.0;
}

void speex_echo_state_destroy(SpeexEchoState *st)
{
   spx_fft_destroy(st->fft_table);
   icm_free(st->e);
   icm_free(st->x);
   icm_free(st->input);
   icm_free(st->y);
   icm_free(st->Yf);
   icm_free(st->Rf);
   icm_free(st->Xf);
   icm_free(st->Yh);
   icm_free(st->Eh);
   icm_free(st->X);
   icm_free(st->Y);
   icm_free(st->E);
   icm_free(st->W);
   icm_free(st->foreground);
   icm_free(st->PHI);
   icm_free(st->power);
   icm_free(st->power_1);
   icm_free(st->window);
   icm_free(st->prop);
   icm_free(st->wtmp);
   icm_free(st->memX);
   icm_free(st->memD);
   icm_free(st->memE);
   icm_free(st->notch_mem);
   icm_free(st);
}

void speex_echo_cancellation(SpeexEchoState *st,const OsInt16 *in,const OsInt16 *far_end,OsInt16 *out)
{
    OsInt32 i,j,chan,speak;
    OsInt32 N,M,C,K;
    OsFloat Syy,See,Sxx,Sdd,Sff;
    OsFloat Dbf;
    OsInt32 update_foreground;
    OsFloat Sey;
    OsFloat ss, ss_1;
    OsFloat Pey = 1.0,Pyy=1.0;
    OsFloat alpha, alpha_1;
    OsFloat RER;
    OsFloat tmp32;

    N = st->window_size;
    M = st->M;
    C = st->C;
    K = st->K;

    st->cancel_count++;
    ss=.35/M;
    ss_1 = 1-ss;

    for (chan = 0; chan < C; chan++)
    {
//      filter_dc_notch16(in+chan, st->notch_radius, st->input+chan*st->frame_size, st->frame_size, st->notch_mem+2*chan, C);
        for(i = 0; i < st->frame_size; i++)
        {
            st->input[chan*st->frame_size+i] = in[i*C+chan];
        }
        // Copy input data to buffer and apply pre-emphasis
        for (i = 0; i < st->frame_size; i++)
        {
            OsFloat tmp32 = st->input[chan*st->frame_size+i] - st->preemph*st->memD[chan];
            st->memD[chan] = st->input[chan*st->frame_size+i];
            st->input[chan*st->frame_size+i] = tmp32;
        }
    }

    for (speak = 0; speak < K; speak++)
    {
        for (i = 0; i < st->frame_size; i++)
        {
            OsFloat tmp32;
            st->x[speak*N+i] = st->x[speak*N+i+st->frame_size];
            tmp32 = far_end[i*K+speak] - st->preemph*st->memX[speak];
            st->x[speak*N+i+st->frame_size] = tmp32;
            st->memX[speak] = far_end[i*K+speak];
        }
    }   

    for (speak = 0; speak < K; speak++)
    {
        // Shift memory: this could be optimized eventually
        for (j = M-1; j >= 0; j--)
        {
            for (i=0;i<N;i++)
                st->X[(j+1)*N*K+speak*N+i] = st->X[j*N*K+speak*N+i];
        }
        // Convert x (echo input) to frequency domain
        spx_fft(st->fft_table,st->x+speak*N,&st->X[speak*N]);
    }

    Sxx = 0;
    for (speak = 0; speak < K; speak++)
    {
        Sxx += mdf_inner_prod(st->x+speak*N+st->frame_size,st->x+speak*N+st->frame_size,st->frame_size);
        power_spectrum_accum(st->X+speak*N,st->Xf,N);
    }

    Sff = 0;  
    for (chan = 0; chan < C; chan++)
    {
        // Compute foreground filter
        spectral_mul_accum(st->X,st->foreground+chan*N*K*M,st->Y+chan*N,N,M*K);
        spx_ifft(st->fft_table,st->Y+chan*N,st->e+chan*N);  // 这里e为前景滤波器的输出
        for (i = 0; i < st->frame_size; i++)
        {
            st->e[chan*N+i] = st->input[chan*st->frame_size+i] - st->e[chan*N+i+st->frame_size];
        }
        Sff += mdf_inner_prod(st->e+chan*N,st->e+chan*N,st->frame_size);
    }

    // Adjust proportional adaption rate
    if (st->adapted) mdf_adjust_prop(st->W,N,M,C*K,st->prop);
    
    // Compute weight gradient and update filter
    if (st->saturated == 0)
    {
        for (chan = 0; chan < C; chan++)
        {
            for (speak = 0; speak < K; speak++)
            {
                for (j = M-1; j >= 0; j--)
                {
                    weighted_spectral_mul_conj(st->power_1,st->prop[j],&st->X[(j+1)*N*K+speak*N],st->E+chan*N,st->PHI,N);
                    for (i = 0; i < N; i++)
                    {
                        st->W[chan*N*K*M + j*N*K + speak*N + i] += st->PHI[i];
                    }
                }
            }
        }
    }
    else
    {
        st->saturated--;
    }

    // Update weight to prevent circular convolution (MDF / AUMDF)
    for (chan = 0; chan < C; chan++)
    {
        for (speak = 0; speak < K; speak++)
        {
            for (j = 0; j < M; j++)
            {
                // This is a variant of the Alternatively Updated MDF (AUMDF) Remove the "if" to make this an MDF filter
                if (j == 0 || st->cancel_count%(M-1) == j-1)
                {
                    spx_ifft(st->fft_table,&st->W[chan*N*K*M + j*N*K + speak*N],st->wtmp);
                    for (i = st->frame_size; i < N; i++)
                    {
                        st->wtmp[i] = 0;
                    }
                    spx_fft(st->fft_table,st->wtmp,&st->W[chan*N*K*M + j*N*K + speak*N]);
                }
            }
        }
    }

    // So we can use power_spectrum_accum
    for (i = 0; i <= st->frame_size; i++)
    {
        st->Rf[i] = st->Yf[i] = st->Xf[i] = 0;
    }

    Dbf = 0; See = 0;
    // Difference in response, this is used to estimate the variance of our residual power estimate
    for (chan = 0; chan < C; chan++)
    {
        spectral_mul_accum(st->X,st->W+chan*N*K*M,st->Y+chan*N,N,M*K);
        spx_ifft(st->fft_table,st->Y+chan*N,st->y+chan*N);                                      // 这里y为背景滤波器的输出
        
        for (i=0;i<st->frame_size;i++)
        {
            st->e[chan*N+i] = st->e[chan*N+i+st->frame_size] - st->y[chan*N+i+st->frame_size];  // 计算两次滤波的误差
        }
        Dbf += (10 + mdf_inner_prod(st->e+chan*N,st->e+chan*N,st->frame_size));                 // 两次滤波之间的误差的功率(Dbf)

        for (i = 0; i < st->frame_size; i++)
        {
            st->e[chan*N+i] = st->input[chan*st->frame_size+i] - st->y[chan*N+i+st->frame_size];
        }
        See += mdf_inner_prod(st->e+chan*N,st->e+chan*N,st->frame_size);                        // 调整系数后的滤波误差功率（See）
    }

     
     // Logic for updating the foreground filter
     // For two time windows, compute the mean of the energy difference, as well as the variance
     // Sff  前景滤波后的误差功率谱
     // See  背景滤波后的误差功率谱
     // Dbf  前景滤波与背景滤波输出误差的功率
    st->Davg1 = 0.6*st->Davg1 + 0.4*(Sff-See);
    st->Davg2 = 0.85*st->Davg2 + 0.15*(Sff-See);
    st->Dvar1 = 0.36*st->Dvar1 + 0.16*Sff*Dbf;
    st->Dvar2 = 0.7225*st->Dvar2 + 0.0225*Sff*Dbf;

    update_foreground = 0;
    // Check if we have a statistically significant reduction in the residual echo
    // Note that this is *not* Gaussian, so we need to be careful about the longer tail
    if ((Sff-See)*fabs(Sff-See) > Sff*Dbf)
        update_foreground = 1;
    else if (st->Davg1 * fabs(st->Davg1) > VAR1_UPDATE*(st->Dvar1))
        update_foreground = 1;
    else if (st->Davg2*fabs(st->Davg2) > VAR2_UPDATE*(st->Dvar2))
        update_foreground = 1;

    if (update_foreground)
    {
        st->Davg1 = st->Davg2 = 0;
        st->Dvar1 = st->Dvar2 = 0.0;
        // Copy background filter to foreground filter
        for (i = 0; i < N*M*C*K; i++)
        {
            st->foreground[i] = st->W[i];
        }
        // Apply a smooth transition so as to not introduce blocking artifacts
        for (chan = 0; chan < C; chan++)
        {
            for (i = 0; i < st->frame_size; i++)
            {
                st->e[chan*N+i+st->frame_size] = st->window[i+st->frame_size]*st->e[chan*N+i+st->frame_size] + st->window[i]*st->y[chan*N+i+st->frame_size];
            }
        }
    }
    else
    {
        OsInt32 reset_background = 0;
        // Otherwise, check if the background filter is significantly worse
        if (-(Sff-See)*fabs(Sff-See) > (VAR_BACKTRACK*Sff*Dbf))
            reset_background = 1;
        if (-st->Davg1*fabs(st->Davg1) > (VAR_BACKTRACK*st->Dvar1))
            reset_background = 1;
        if (-st->Davg2*fabs(st->Davg2) > (VAR_BACKTRACK*st->Dvar2))
            reset_background = 1;
        if (reset_background)
        {
            // Copy foreground filter to background filter
            for (i = 0; i < N*M*C*K; i++)
            {
                st->W[i] = st->foreground[i];
            }
            // We also need to copy the output so as to get correct adaptation
            for (chan = 0; chan < C; chan++)
            {        
                for (i = 0; i < st->frame_size; i++)
                    st->y[chan*N+i+st->frame_size] = st->e[chan*N+i+st->frame_size];
                for (i = 0; i < st->frame_size; i++)
                    st->e[chan*N+i] = st->input[chan*st->frame_size+i] - st->y[chan*N+i+st->frame_size];
            }        
            See = Sff;
            st->Davg1 = st->Davg2 = 0;
            st->Dvar1 = st->Dvar2 = 0.0;
        }
    }

    Sey = Syy = Sdd = 0;  
    for (chan = 0; chan < C; chan++)
    {    
        // Compute error signal (for the output with de-emphasis)
        for (i = 0; i < st->frame_size; i++)
        {
            OsFloat tmp_out = st->input[chan*st->frame_size+i] - st->e[chan*N+i+st->frame_size];
            tmp_out = tmp_out + st->preemph * st->memE[chan];   // deemphasis
            // This is an arbitrary test for saturation in the microphone signal
            if (in[i*C+chan] <= -32000 || in[i*C+chan] >= 32000)
            {
                if (st->saturated == 0)
                    st->saturated = 1;
            }
            out[i*C+chan]   = sat(floor(0.5+tmp_out),-32768,32767);
            st->memE[chan]  = tmp_out;
        }

        // Compute error signal (filter update version)
        for (i = 0; i < st->frame_size; i++)
        {
            st->e[chan*N+i+st->frame_size] = st->e[chan*N+i];
            st->e[chan*N+i] = 0;
        }

        // Compute a bunch of correlations
        Sey += mdf_inner_prod(st->e+chan*N+st->frame_size,st->y+chan*N+st->frame_size,st->frame_size);
        Syy += mdf_inner_prod(st->y+chan*N+st->frame_size,st->y+chan*N+st->frame_size,st->frame_size);
        Sdd += mdf_inner_prod(st->input+chan*st->frame_size,st->input+chan*st->frame_size,st->frame_size);

        // Convert error to frequency domain
        spx_fft(st->fft_table,st->e+chan*N,st->E+chan*N);   // E(k) = FFT[0; e(k)]
        for (i = 0; i < st->frame_size; i++)
            st->y[i+chan*N] = 0;
        spx_fft(st->fft_table,st->y+chan*N,st->Y+chan*N);

        // Compute power spectrum of echo (X), error (E) and filter response (Y)
        power_spectrum_accum(st->E+chan*N,st->Rf,N);
        power_spectrum_accum(st->Y+chan*N,st->Yf,N);
    }

    //printf ("%f %f %f %f\n", Sff, See, Syy, Sdd, st->update_cond);

    // Do some sanity check
    if (!(Syy >= 0 && Sxx >= 0 && See >= 0) || !(Sff < N*1e9 && Syy < N*1e9 && Sxx < N*1e9))
    {
        // Things have gone really bad
        st->screwed_up += 50;
        for (i = 0; i < st->frame_size*C; i++)
            out[i] = 0;
    }
    else if (Sff > (Sdd + N*10000))
    {
        // AEC seems to add lots of echo instead of removing it, let's see if it will improve
        st->screwed_up++;
    }
    else
    {
        st->screwed_up = 0; // Everything's fine
    }
    if (st->screwed_up >= 50)
    {
        OsLog("The echo canceller started acting funny and got slapped (reset). It swears it will behave now.");
        speex_echo_state_reset(st);
        return;
    }

    // Add a small noise floor to make sure not to have problems when dividing 
    See = max(See,N*100);

    for (speak = 0; speak < K; speak++)
    {
        Sxx += mdf_inner_prod(st->x+speak*N+st->frame_size, st->x+speak*N+st->frame_size, st->frame_size);
        power_spectrum_accum(st->X+speak*N, st->Xf, N);
    }


    // Smooth far end energy estimate over time
    for (j = 0; j <= st->frame_size; j++)
        st->power[j] = ss_1*st->power[j] + 1 + ss*st->Xf[j];

    // Compute filtered spectra and (cross-)correlations
    for (j = st->frame_size; j >= 0; j--)
    {
        OsFloat Eh, Yh;
        Eh = st->Rf[j] - st->Eh[j];
        Yh = st->Yf[j] - st->Yh[j];
        Pey = Pey + Eh*Yh;
        Pyy = Pyy + Yh*Yh;
        st->Eh[j] = (1-st->spec_average)*st->Eh[j] + st->spec_average*st->Rf[j];
        st->Yh[j] = (1-st->spec_average)*st->Yh[j] + st->spec_average*st->Yf[j];
    }

    Pyy = sqrt(Pyy);
    Pey = Pey/Pyy;

    // Compute correlation updatete rate
    tmp32 = st->beta0*Syy;
    if (tmp32 > st->beta_max*See)
        tmp32 = st->beta_max*See;
    alpha   = tmp32 / See;
    alpha_1 = 1.0 - alpha;
    // Update correlations (recursive average)
    st->Pey = alpha_1*st->Pey + alpha*Pey;
    st->Pyy = alpha_1*st->Pyy + alpha*Pyy;
    if (st->Pyy < 1.0)
        st->Pyy = 1.0;
    // We don't really hope to get better than 33 dB (MIN_LEAK-3dB) attenuation anyway
    if (st->Pey < MIN_LEAK*st->Pyy)
        st->Pey = MIN_LEAK*st->Pyy;
    if (st->Pey > st->Pyy)
        st->Pey = st->Pyy;
    // leak_estimate is the linear regression result
    st->leak_estimate = st->Pey/st->Pyy;
    // This looks like a stupid bug, but it's right (because we convert from Q14 to Q15)
    if (st->leak_estimate > 16383)
        st->leak_estimate = 32767;
    else
        st->leak_estimate = st->leak_estimate;
    // printf ("%f\n", st->leak_estimate)

    // Compute Residual to Error Ratio
    RER = (0.0001*Sxx + 3.0*(st->leak_estimate*Syy)) / See;
    // Check for y in e (lower bound on RER)
    if (RER < Sey*Sey/(1+See*Syy))
        RER = Sey*Sey/(1+See*Syy);
    if (RER > 0.5)
        RER = 0.5;

    // We consider that the filter has had minimal adaptation if the following is true
    if (!st->adapted && (st->sum_adapt > M) && (st->leak_estimate*Syy > 0.03f*Syy))
    {
        st->adapted = 1;
    }

    if (st->adapted)
    {
        // Normal learning rate calculation once we're past the minimal adaptation phase
        for (i = 0; i <= st->frame_size; i++)
        {
            OsFloat r,e;
            // Compute frequency-domain adaptation mask
            r = st->leak_estimate*st->Yf[i];
            e = st->Rf[i]+1;

            if (r > 0.5*e) r = 0.5*e;
            r = 0.7*r + 0.3*RER*e;
            // st->power_1[i] = adapt_rate*r/(e*(1+st->power[i]))
            st->power_1[i] = r/(e*(st->power[i]+10));
        }
    }
    else
    {
        // Temporary adaption rate if filter is not yet adapted enough
        OsFloat adapt_rate = 0;

        if (Sxx > N*1000)
        {
            tmp32 = 0.25f*Sxx;
            if (tmp32 > 0.25*See)
                tmp32 = 0.25*See;
            adapt_rate = tmp32 / See;
        }
        for (i = 0; i <= st->frame_size; i++)
            st->power_1[i] = adapt_rate/(st->power[i]+10);
        // How much have we adapted so far?
        st->sum_adapt = st->sum_adapt + adapt_rate;
    }
}

OsInt32 speex_echo_ctl(SpeexEchoState *st,OsInt32 request,void *ptr)
{
    switch(request)
    {
    case SPEEX_ECHO_GET_FRAME_SIZE:
        (*(OsInt32*)ptr) = st->frame_size;
        break;
    case SPEEX_ECHO_SET_SAMPLING_RATE:
        st->sampling_rate   = (*(OsInt32*)ptr);
        st->spec_average    = 1.0*st->frame_size / st->sampling_rate;
        st->beta0           = (2.0f*st->frame_size)/st->sampling_rate;
        st->beta_max        = (.5f*st->frame_size)/st->sampling_rate;

        if (st->sampling_rate < 12000)
            st->notch_radius = 0.9;
        else if (st->sampling_rate < 24000)
            st->notch_radius = 0.982;
        else
            st->notch_radius = 0.992;
        break;
    case SPEEX_ECHO_GET_SAMPLING_RATE:
        (*(OsInt32*)ptr) = st->sampling_rate;
        break;
    case SPEEX_ECHO_GET_IMPULSE_RESPONSE_SIZE:
        *((OsInt32 *)ptr) = st->M * st->frame_size;
        break;
    case SPEEX_ECHO_GET_IMPULSE_RESPONSE:
        {
            OsInt32 M = st->M, N = st->window_size, n = st->frame_size, i, j;
            OsInt32 *filt = (OsInt32 *)ptr;
            for(j = 0; j < M; j++)
            {
                spx_ifft(st->fft_table,&st->W[j*N],st->wtmp);

                for(i = 0; i < n; i++)
                    filt[j*n+i] = 32767*st->wtmp[i];
            }
        }
        break;
    default:
        OsLog("Unknown speex_echo_ctl request: ",request);
        return -1;
    }
    return 0;
}