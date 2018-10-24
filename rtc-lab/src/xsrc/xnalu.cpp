

#include <stdlib.h>
#include <string.h>
#include "xnalu.h"

xnalu_spspps_t * xnalu_spspps_create(){
	xnalu_spspps_t * obj = (xnalu_spspps_t *)malloc(sizeof(xnalu_spspps_t));
	xnalu_spspps_init(obj);
	return obj;
}

void xnalu_spspps_delete(xnalu_spspps_t * obj){
	if(!obj) return;
	xnalu_spspps_close(obj);
	free(obj);
}

int xnalu_spspps_init(xnalu_spspps_t * obj){
	memset(obj, 0, sizeof(sizeof(xnalu_spspps_t)));
	obj->sps.size = SPS_OR_PPS_BUF_SIZE;
	obj->sps.ptr = (uint8_t *) malloc(obj->sps.size);
	obj->pps.size = SPS_OR_PPS_BUF_SIZE;
	obj->pps.ptr = (uint8_t *) malloc(obj->pps.size);
	return 0;
}

void xnalu_spspps_close(xnalu_spspps_t * obj){
	if(obj->sps.ptr){
		free(obj->sps.ptr);
		obj->sps.ptr = 0;
	}

	if (obj->pps.ptr) {
		free(obj->pps.ptr);
		obj->pps.ptr = 0;
	}

	xnalu_spspps_reset(obj);

}

void xnalu_spspps_reset(xnalu_spspps_t * obj){
	obj->sps.length = 0;
	obj->pps.length = 0;
	obj->seq = 0;
}

static
int xnalu_spspps_process(xnalu_spspps_t * obj
		, xsbuf_t * the
		, xsbuf_t * other
		, const uint8_t * nalu, uint32_t nalu_len){

	if(nalu_len == 0) return 0;

	if (the->length > 0) {
		if (the->length == nalu_len) {
			if (memcmp(the->ptr, nalu, nalu_len) == 0) {
				return 0;
			}
		}

		// new one
		the->length = 0;
		other->length = 0;
	}

	if (nalu_len > the->size) {
		free(the->ptr);
		the->size = nalu_len;
		the->ptr = (uint8_t *) malloc(the->size);
	}
	memcpy(the->ptr, nalu, nalu_len);
	the->length = nalu_len;

	if(other->length == 0){
		return 0;
	}
	obj->seq++;
	return obj->seq;
}

int xnalu_spspps_input(xnalu_spspps_t * obj , const uint8_t * nalu, uint32_t nalu_len){
	unsigned nal_type = xnalu_type(nalu);
	if(nal_type == NALU_TYPE_SPS){
		return xnalu_spspps_process(obj, &obj->sps, &obj->pps, nalu, nalu_len);
	}else if(nal_type == NALU_TYPE_PPS){
		return xnalu_spspps_process(obj, &obj->pps, &obj->sps, nalu, nalu_len);
	}else{
		return -1;
	}
}


