

#ifndef __RAV_RECORD_H__
#define __RAV_RECORD_H__

#include <stdio.h>
#include "media_hapi.h"

#ifdef __cplusplus
extern "C"{
#endif
    
typedef struct ravrecord * ravrecord_t;

typedef enum{
	RR_FMT_NONE,
	RR_FMT_RTP_VP8,
	RR_FMT_MAX,
}RR_FMT_T;
//    typedef enum{
//        MCODEC_ID_NONE = 0,
//        MCODEC_ID_UNKNOWN,
//        MCODEC_ID_I420,
//        MCODEC_ID_VP8,
//        MCODEC_ID_VP9,
//        MCODEC_ID_H264,
//        MCODEC_ID_PCM,
//        MCODEC_ID_OPUS,
//        MCODEC_ID_ILBC,
//        MCODEC_ID_ISAC,
//        MCODEC_ID_G722,
//        MCODEC_ID_MAX
//    }mediacodec_id_t;


ravrecord_t rr_open(const char * output_file_name
	, mediacodec_id_t video_codec_id, int width, int height, int fps
	, mediacodec_id_t audio_codec_id, int audio_samplerate, int audio_channels,int playout_samplerate,int playout_channels
        ,unsigned char *sps_pps_buffer, int  sps_pps_length);
void rr_close(ravrecord_t rr);
int rr_process_video(ravrecord_t rr, int64_t pts, unsigned char * buf, int length);
int rr_process_audio(ravrecord_t rr, int64_t pts, unsigned char * buf, int length);
int rr_process_pcm(ravrecord_t rr, int64_t pts, unsigned char * buf, int length);

    
#ifdef __cplusplus
} // extern "C"
#endif
        
#endif
