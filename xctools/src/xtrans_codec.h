#ifndef __XTRANS_CODEC_H__
#define __XTRANS_CODEC_H__

#include "xrtp_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XCODEC_H264 RTP_PT_H264
#define XCODEC_OPUS RTP_PT_OPUS
#define XCODEC_SPEEX RTP_PT_SPEEX

typedef struct audio_codec_param{
	uint32_t sample_rate;
	uint32_t channels;
	uint32_t bits;
}audio_codec_param;
#define audio_codec_param_build(sr, ch, b) {sr, ch, b}

typedef int (*codec_context_size)(const audio_codec_param * param);
typedef int (*codec_init)(void * ctx, const audio_codec_param * param);
typedef void (*codec_destory)(void * ctx);
typedef int (*codec_frame_range)(void * ctx, int *min, int *max, int *recommend);
typedef int (*codec_process)(void * ctx, const void * in_data, uint32_t in_length, void * out_data, uint32_t max_out_size);

typedef struct audio_codec_ops{
	codec_context_size context_size;
	codec_init init;
	codec_destory destory;
	codec_frame_range frame_range;
	codec_process process;
}audio_codec_ops;


typedef struct audio_transcoder_s audio_transcoder_t;

int audio_transcoder_get_size(const audio_codec_param * param, int src_codec, int dst_codec, int dst_frame_size);
audio_transcoder_t * audio_transcoder_create(const audio_codec_param * param, int src_codec, int dst_codec, int dst_frame_size);
void audio_transcoder_free(audio_transcoder_t * obj);
int audio_transcoder_init(audio_transcoder_t * obj, const audio_codec_param * param, int src_codec, int dst_codec, int dst_frame_size);
void audio_transcoder_close(audio_transcoder_t * obj);
int audio_transcoder_input(audio_transcoder_t * obj
		, uint32_t in_timestamp, const uint8_t * in_data, uint32_t in_length);
int audio_transcoder_next(audio_transcoder_t * obj
		, uint32_t *out_timestamp, uint8_t * out_data, uint32_t max_out_size);

#ifdef __cplusplus
}
#endif

#endif
