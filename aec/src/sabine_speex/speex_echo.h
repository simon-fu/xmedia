/* Copyright (C) Jean-Marc Valin */
#ifndef SPEEX_ECHO_H
#define SPEEX_ECHO_H

#include "Platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Obtain frame size used by the AEC */
#define SPEEX_ECHO_GET_FRAME_SIZE 3
/** Set sampling rate */
#define SPEEX_ECHO_SET_SAMPLING_RATE 24
/** Get sampling rate */
#define SPEEX_ECHO_GET_SAMPLING_RATE 25
/* Can't set window sizes */
/** Get size of impulse response (int32) */
#define SPEEX_ECHO_GET_IMPULSE_RESPONSE_SIZE 27
/* Can't set window content */
/** Get impulse response (int32[]) */
#define SPEEX_ECHO_GET_IMPULSE_RESPONSE 29


typedef struct SpeexEchoState_ SpeexEchoState;

/** Creates a new echo canceller state
 * @param frame_size Number of samples to process at one time (should correspond to 10-20 ms)
 * @param filter_length Number of samples of echo to cancel (should generally correspond to 100-500 ms)
 * @return Newly-created echo canceller state
 */
SpeexEchoState *speex_echo_state_init(OsInt32 frame_size, OsInt32 filter_length);
/** Creates a new multi-channel echo canceller state
 * @param frame_size Number of samples to process at one time (should correspond to 10-20 ms)
 * @param filter_length Number of samples of echo to cancel (should generally correspond to 100-500 ms)
 * @param nb_mic Number of microphone channels
 * @param nb_speakers Number of speaker channels
 * @return Newly-created echo canceller state
 */
SpeexEchoState *speex_echo_state_init_mc(OsInt32 frame_size, OsInt32 filter_length, OsInt32 nb_mic, OsInt32 nb_speakers);
/** Destroys an echo canceller state 
 * @param st Echo canceller state
*/
void speex_echo_state_destroy(SpeexEchoState *st);
/** Performs echo cancellation a frame, based on the audio sent to the speaker (no delay is added
 * to playback in this form)
 *
 * @param st Echo canceller state
 * @param rec Signal from the microphone (near end + far end echo)
 * @param play Signal played to the speaker (received from far end)
 * @param out Returns near-end signal with echo removed
 */
void speex_echo_cancellation(SpeexEchoState *st,const OsInt16 *rec,const OsInt16 *play,OsInt16 *out);
/** Reset the echo canceller to its original state 
 * @param st Echo canceller state
 */
void speex_echo_state_reset(SpeexEchoState *st);
/**
 * Used like the ioctl function to control the echo canceller parameters
 * @param st Echo canceller state
 * @param request ioctl-type request (one of the SPEEX_ECHO_* macros)
 * @param ptr Data exchanged to-from function
 * @return 0 if no error, -1 if request in unknown
 */
OsInt32 speex_echo_ctl(SpeexEchoState *st,OsInt32 request,void *ptr);



typedef struct SpeexDecorrState_ SpeexDecorrState;

/**
 * Create a state for the channel decorrelation algorithm This is useful for multi-channel echo cancellation only 
 * @param rate Sampling rate
 * @param channels Number of channels (it's a bit pointless if you don't have at least 2)
 * @param frame_size Size of the frame to process at ones (counting samples *per* channel)
*/
SpeexDecorrState *speex_decorrelate_new(OsInt32 rate,OsInt32 channels,OsInt32 frame_size);
/**
 * Remove correlation between the channels by modifying the phase and possibly adding noise in a way that is not (or little) perceptible.
 * @param st Decorrelator state
 * @param in Input audio in interleaved format
 * @param out Result of the decorrelation (out *may* alias in)
 * @param strength How much alteration of the audio to apply from 0 to 100.
*/
void speex_decorrelate(SpeexDecorrState *st,const OsInt16 *in,OsInt16 *out,OsInt32 strength);
void speex_decorrelate_destroy(SpeexDecorrState *st);

#ifdef __cplusplus
}
#endif

#endif