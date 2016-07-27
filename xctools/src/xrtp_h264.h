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

	uint8_t fu_head[4];
	uint32_t fu_head_len;
	uint32_t fu_flag_offset;
}xnalu_to_rtp_t;
int xnalu_to_rtp_init(xnalu_to_rtp_t * ctx, uint32_t ssrc, uint32_t payload_type, uint32_t max_rtp_size);
int xnalu_to_rtp_first(xnalu_to_rtp_t * ctx, uint32_t timestamp, const uint8_t *nalu, uint32_t nalu_len);
int xnalu_to_rtp_next(xnalu_to_rtp_t * ctx, uint8_t *rtp);




#ifdef __cplusplus
};
#endif


#endif
