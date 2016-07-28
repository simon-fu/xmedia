#ifndef __XRTP_H264_H__
#define __XRTP_H264_H__

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>
#include "xrtp_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif




typedef struct xrtp_to_nalu_s{
	uint8_t *internal_nalu_buf;

	uint8_t *nalu_buf;
	uint32_t nalu_buf_size;
	uint32_t nalu_len;
	uint32_t last_seq;
	uint32_t timestamp;
	int m_hasIFrame;
	int m_FrameFlag;
	int m_naltype;
	uint32_t m_rtpseq;
	int last_nalu_done;
}xrtp_to_nalu_t;

int xrtp_to_nalu_init(xrtp_to_nalu_t * ctx, uint8_t *nalu_buf, uint32_t nalu_buf_size, uint32_t nalu_buf_offset);
void xrtp_to_nalu_close(xrtp_to_nalu_t * ctx);
int xrtp_to_nalu_next(xrtp_to_nalu_t * ctx, const uint8_t *rtp, uint32_t rtp_len);


typedef struct xnalu_to_rtp_s{

	uint32_t ssrc;
	uint32_t payload_type;
	uint32_t max_rtp_size;
	uint32_t seq;

	uint32_t timestamp;
	uint32_t nalu_len;
	const uint8_t *pos;
	uint32_t nalu_remains;
	uint32_t nalu_frag_size;
	int is_fu;
	int mask;

	uint8_t fu_head[4];
	uint32_t fu_head_len;
	uint32_t fu_flag_offset;
}xnalu_to_rtp_t;
int xnalu_to_rtp_init(xnalu_to_rtp_t * ctx, uint32_t ssrc, uint32_t payload_type, uint32_t max_rtp_size);
int xnalu_to_rtp_first(xnalu_to_rtp_t * ctx, uint32_t timestamp, const uint8_t *nalu, uint32_t nalu_len, int mask);
int xnalu_to_rtp_next(xnalu_to_rtp_t * ctx, uint8_t *rtp);


typedef struct xtimestamp64{
  uint32_t last_ts_;
  int64_t num_wrap_;
}xtimestamp64;
int xtimestamp64_init(xtimestamp64 * obj);
int64_t xtimestamp64_unwrap(xtimestamp64 * obj, uint32_t ts);


typedef struct xrtp_transformer{
  uint32_t src_samplerate;
  uint32_t dst_samplerate;
  uint32_t dst_payloadtype;
  uint32_t dst_ssrc;
  uint32_t last_seq;
  xtimestamp64 tswrapper;
  int64_t src_first_timestamp;
}xrtp_transformer;
void xrtp_transformer_init(xrtp_transformer * obj, uint32_t src_samplerate, uint32_t dst_samplerate, uint32_t dst_payloadtype, uint32_t dst_ssrc);
void xrtp_transformer_process(xrtp_transformer * obj, unsigned char * rtp);


typedef struct xrtp_h264_repacker{
	uint32_t src_samplerate;
	uint32_t dst_samplerate;
	uint8_t * nalu_buf;
	xrtp_to_nalu_t rtp2nalu;
	xnalu_to_rtp_t nalu2rtp;

	int last_nalu_len;
	// int last_mask;
	// int last_timestamp;
	xtimestamp64 tswrapper;
	int64_t src_first_timestamp;

	int got_sps;
	int got_pps;
	int got_keyframe;
}xrtp_h264_repacker;
void xrtp_h264_repacker_init(xrtp_h264_repacker * obj
		, uint8_t *nalu_buf, uint32_t nalu_buf_size, uint32_t nalu_buf_offset
		, uint32_t dst_ssrc, uint32_t dst_payloadtype, uint32_t max_rtp_size
		, uint32_t src_samplerate, uint32_t dst_samplerate);
int xrtp_h264_repacker_input(xrtp_h264_repacker * obj, unsigned char * rtp, int rtp_len);
int xrtp_h264_repacker_next(xrtp_h264_repacker * obj, unsigned char * rtp);


#ifdef __cplusplus
};
#endif


#endif
