#ifndef __EVRTMP_CLIENT_H__
#define __EVRTMP_CLIENT_H__

#include <stddef.h>
#include "libevrtmp/rtmp.h"

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct evrtmp_client_s evrtmp_client_t;

typedef int (*evrtmp_client_cb_on_nalu) (evrtmp_client_t *e, uint32_t pts, const uint8_t * nalu, uint32_t nalu_len);
typedef int (*evrtmp_client_cb_on_speex)(evrtmp_client_t *e, uint32_t pts, const uint8_t * data, uint32_t data_len);

typedef struct evrtmp_client_callback{
	evrtmp_client_cb_on_nalu     on_nalu;
	evrtmp_client_cb_on_speex    on_speex;
}evrtmp_client_callback;


evrtmp_client_t * evrtmp_client_create(evrtmp_client_callback * cb, struct event_base * ev_base);

void evrtmp_client_delete(evrtmp_client_t * obj);

void evrtmp_client_set_user_context(evrtmp_client_t * obj, void * user_context);

void * evrtmp_client_get_user_context(evrtmp_client_t *obj);

void evrtmp_client_close(evrtmp_client_t * obj);

int evrtmp_client_connect(evrtmp_client_t * obj, const char * url, int is_publish);

int evrtmp_client_process_meta(evrtmp_client_t * obj
		, uint32_t timestamp, const uint8_t * nalu, uint32_t nalu_len);


#define EVRTMP_AVC_HEAD 9
#define EVRTMP_NALU_OFFSET (EVRTMP_AVC_HEAD + RTMP_MAX_HEADER_SIZE)
#define EVRTMP_SPEEX_HEAD 1
#define EVRTMP_SPEEX_OFFSET (EVRTMP_SPEEX_HEAD + RTMP_MAX_HEADER_SIZE)

// nalu_offset >= EVRTMP_NALU_OFFSET
int evrtmp_client_send_nalu(evrtmp_client_t * obj, uint32_t timestamp, uint8_t * buf, uint32_t nalu_offset, uint32_t nalu_len, uint32_t size);
int evrtmp_client_send_speex(evrtmp_client_t * obj, uint32_t timestamp, uint8_t * buf, uint32_t offset, uint32_t length, uint32_t size);

#ifdef __cplusplus
};
#endif



#endif
