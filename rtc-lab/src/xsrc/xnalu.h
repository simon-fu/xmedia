#ifndef __XNALU_H__
#define __XNALU_H__

#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif


#define NALU_TYPE_SLICE 1
#define NALU_TYPE_DPA 2
#define NALU_TYPE_DPB 3
#define NALU_TYPE_DPC 4
#define NALU_TYPE_IDR 5
#define NALU_TYPE_SEI 6
#define NALU_TYPE_SPS 7
#define NALU_TYPE_PPS 8
#define NALU_TYPE_AUD 9
#define NALU_TYPE_EOSEQ 10
#define NALU_TYPE_EOSTREAM 11
#define NALU_TYPE_FILL 12

// simple buffer
typedef struct xsbuf_s {
	uint8_t 	*ptr;
	uint32_t 	size;
	uint32_t 	length;
}xsbuf_t;



#define SPS_OR_PPS_BUF_SIZE 128
typedef struct xnalu_spspps_s{
	xsbuf_t sps;
	xsbuf_t pps;
	uint32_t seq;
}xnalu_spspps_t;


xnalu_spspps_t * xnalu_spspps_create();

void xnalu_spspps_delete(xnalu_spspps_t * obj);

int xnalu_spspps_init(xnalu_spspps_t * obj);

void xnalu_spspps_close(xnalu_spspps_t * obj);

void xnalu_spspps_reset(xnalu_spspps_t * obj);

int xnalu_spspps_input(xnalu_spspps_t * obj , const uint8_t * nalu, uint32_t nalu_len);

#define xnalu_spspps_sps(x) (&((x)->sps))
#define xnalu_spspps_pps(x) (&((x)->pps))
#define xnalu_spspps_exist(x) ((x)->sps.length > 0 && (x)->pps.length > 0)


#define xnalu_type(x) (*(x)&0x1F)


#ifdef __cplusplus
};
#endif


#endif

