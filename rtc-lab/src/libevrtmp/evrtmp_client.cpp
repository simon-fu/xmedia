#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <iostream>
//#include <signal.h> // SIGINT
//#include <limits.h>
//#include <list>
//#include <map>
//#include <unordered_map>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include <sys/socket.h>
#include <netinet/tcp.h>
//#ifndef WIN32
//#include <arpa/inet.h> // inet_ntoa
//#endif

// #include "plw.h"
#include "libevrtmp/rtmp.h"
#include "xnalu.h"
#include "evrtmp_client.h"
#include "xrtp_h264.h"

#define MNAME "<evrtmp> "
#include "dbg_defs.h"

#undef dbgd
#define dbgd(...)
//#define dbgd(...) do{printf(__VA_ARGS__); printf("\n");}while(0)
//#define dbgi(...) do{printf(__VA_ARGS__); printf("\n");}while(0)
//#define dbge(...) do{printf(__VA_ARGS__); printf("\n");}while(0)



//#define SPS_OR_PPS_BUF_SIZE 128
struct evrtmp_client_s{
	void * user_context;
	struct event_base * ev_base;
	evrtmp_client_callback cb;
	NBRTMP   rtmp_stor;
	NBRTMP * rtmp;
	int is_publish;
	struct bufferevent * bev;
	struct event *evtimer;
	RTMPPacket packet4send;

	xnalu_spspps_t spspps;
	char *url;

	int connect_try;

//	uint32_t sps_len;
//	uint32_t pps_len;
//	uint8_t sps[SPS_OR_PPS_BUF_SIZE];
//	uint8_t pps[SPS_OR_PPS_BUF_SIZE];

	int64_t last_video_ts;
	int64_t last_audio_ts;

};

static int kick_connect(evrtmp_client_t * obj);


static void evrtmp_readcb(struct bufferevent *bev, void *ctx)
{
	evrtmp_client_t * obj = (evrtmp_client_t *) ctx;
	struct evbuffer *input = bufferevent_get_input(bev);
	dbgd("evrtmp_readcb: -> bev=%p, ctx=%p, length=%d", bev, ctx, (int)evbuffer_get_length(input));
	int oldplaying = NBRTMP_IsPlaying(obj->rtmp) ;
	if(!NBRTMP_Recv(obj->rtmp)){
		dbge("something wrong with NBRTMP_Recv ");
	}
	int newplaying = NBRTMP_IsPlaying(obj->rtmp) ;
	if(newplaying != oldplaying){
		dbgi("playing update %d -> %d\n", oldplaying, newplaying);
	}
	dbgd("evrtmp_readcb: <- ");
}

static void evrtmp_eventcb(struct bufferevent *bev, short events, void *ctx) {
	evrtmp_client_t * obj = (evrtmp_client_t *) ctx;
	int one = 1;
	dbgd("evrtmp_eventcb: -> bev=%p, events=0x%04X, ctx=%p\n", bev, events, ctx);
	if (events & BEV_EVENT_CONNECTED) {
		dbgi("connected to %s\n", obj->rtmp->url);
		evutil_socket_t fd = bufferevent_getfd(bev);

		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
		bufferevent_enable(bev, EV_READ);
		NBRTMP_Connect(obj->rtmp, bufferevent_getfd(bev));

		if(obj->evtimer){
			event_free(obj->evtimer);
			obj->evtimer = NULL;
		}
	} else if (events & BEV_EVENT_ERROR) {
		dbgi("disconnect from %s\n", obj->rtmp->url);
		evrtmp_client_close(obj);
		kick_connect(obj);
	} else {
		dbgi("sth wrong -> bev=%p, events=0x%04X, ctx=%p\n", bev, events, ctx);
		evrtmp_client_close(obj);
		kick_connect(obj);
	}
	dbgd("evrtmp_eventcb: <- \n");
}

static void conn_timeoutcb(evutil_socket_t fd, short what, void *arg) {
	evrtmp_client_t * obj = (evrtmp_client_t *) arg;
	dbgi("connect timeout, url=%s\n", obj->rtmp->url);
	evrtmp_client_close(obj);
	kick_connect(obj);
}


static int
on_rtmp_send(NBRTMP *rtmp, void * user_context, unsigned char *data, unsigned length)
{
	evrtmp_client_t * obj = (evrtmp_client_t *) user_context;
	int ret;
	dbgd("on send data: data=%p, length=%d\n", data, length);
	ret = bufferevent_write(obj->bev, data, length);
	if(ret == 0){
		return length;
	}
	dbge("bufferevent_write error %d", ret);
	return -1;
}

static int
on_rtmp_recv(NBRTMP *rtmp, void * user_context, unsigned char *data, unsigned size, unsigned * length)
{
	evrtmp_client_t * obj = (evrtmp_client_t *) user_context;
	int ret;
	struct evbuffer *input = bufferevent_get_input(obj->bev);
	dbgd("on recv data: -> data=%p, size=%d, available=%d\n", data, size, (int)evbuffer_get_length(input));
	ret = evbuffer_remove(bufferevent_get_input(obj->bev), data, size);
	if(ret >= 0){
		*length = ret;
	}else{
		*length = 0;
	}
	dbgd("on recv data: <- ret=%d, length=%d\n", ret, *length);
	return *length;
}

static int
on_rtmp_packet(NBRTMP *rtmp, void * user_context, RTMPPacket * packet)
{
	evrtmp_client_t * obj = (evrtmp_client_t *) user_context;
	uint8_t * p = (uint8_t *) packet->m_body;
//	dbgd("on_rtmp_packet: ==>> typ=%d, ts=%d(abs=%d), len=%d/%d\n",
//			packet->m_packetType, packet->m_nTimeStamp, packet->m_hasAbsTimestamp, packet->m_nBytesRead, packet->m_nBodySize);

	if(packet->m_nBodySize <= 0){
		return 0;
	}

	//		dbgd("  %02X %02X %02X %02X %02X %02X %02X %02X\n"
	//				, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
	//		dbgd("  %02X %02X %02X %02X %02X %02X %02X %02X\n"
	//				, p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
	//		dbgd("  %02X %02X %02X %02X %02X %02X %02X %02X\n"
	//				, p[16], p[17], p[18], p[19], p[20], p[21], p[22], p[23]);

	if(packet->m_packetType == RTMP_PACKET_TYPE_VIDEO){

		uint8_t frame_type = (p[0] >> 4) & 0x0F;
		uint8_t codec_id = p[0] & 0x0F;

		if(codec_id == RTMP_VIDEO_CODECID_AVC && (frame_type == 1 || frame_type == 2)){
			uint8_t avc_pkt_type = p[1];
			uint32_t comp_time_offset = AMF_DecodeInt24((const char *)p+2);

			if(avc_pkt_type == 0){

				p += 11;
				uint32_t sps_len = AMF_DecodeInt16((const char *)p);
				if(obj->cb.on_nalu){
					(*obj->cb.on_nalu)(obj, packet->m_nTimeStamp, p+2, sps_len);
				}

				dbgd(" sps: len=%d, data=%02X %02X %02X %02X %02X %02X %02X %02X\n", sps_len, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
				p += (2 + sps_len);

				p += 1;
				uint32_t pps_len = AMF_DecodeInt16((const char *)p);
				if (obj->cb.on_nalu) {
					(*obj->cb.on_nalu)(obj, packet->m_nTimeStamp, p+2, pps_len);
				}
				dbgd(" pps: len=%d, data=%02X %02X %02X %02X %02X %02X %02X %02X\n", pps_len, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
				p += (2 + pps_len);

			}
			else if(avc_pkt_type == 1){
				p += 5;
				int remains = packet->m_nBodySize - 5;

				while(remains > 0){
					uint32_t nal_len = AMF_DecodeInt32((const char *)p);
					uint8_t nal_type = p[4]&0x1F;
					dbgd("  nal->: len=%d, type=%d\n", nal_len, nal_type);

					if (nal_type != NALU_TYPE_SEI && nal_type != NALU_TYPE_AUD) {
						if(obj->cb.on_nalu){
							(*obj->cb.on_nalu)(obj, packet->m_nTimeStamp, p + 4, nal_len);
						}
					}

					remains -= (nal_len+4);
					p += (nal_len + 4);
				}
				if(remains < 0){
					dbge("rtmp nalu remains > 0, it is %d\n", remains);
//					exit(0);
				}
			}

		}else{
			dbgi("  unknown video codec packet\n");
		}


	}else if(packet->m_packetType == RTMP_PACKET_TYPE_AUDIO){
		uint8_t codec_id = (p[0] >> 4) & 0x0F;

		if (codec_id == RTMP_AUDIO_CODECID_SPEEX) {
			if (obj->cb.on_speex) {
				dbgd("speex[0]=%02X", p[0]);
				(*obj->cb.on_speex)(obj, packet->m_nTimeStamp, p + 1, packet->m_nBodySize-1);
			}else{
				dbgi("  unknown audio codec %d\n", codec_id);
			}

//			p += 1;
//			int remains = packet->m_nBodySize-1;
//			while (remains > 0) {
//				uint32_t len = AMF_DecodeInt32((const char *) p);
//				remains -= (len + 4);
//				p += (len + 4);
//			}
//			if (remains < 0) {
//				dbge("rtmp audio remains > 0, it is %d\n", remains);
//				exit(0);
//			}


		}
	}

	return 0;
}


static int kick_connect(evrtmp_client_t * obj){

	static NBRTMP_Callback cb = {
			on_rtmp_send,
			on_rtmp_recv,
			on_rtmp_packet
	};

	int ret = 0;
	do{
		if(obj->bev) break;

		if(obj->connect_try <= 0){
			dbgi("give up connecting to %s\n", obj->url);
			ret = -1;
			break;
		}
		obj->connect_try--;

		dbgi("connecting to %s\n", obj->url);

		obj->rtmp = &obj->rtmp_stor;
		NBRTMP_Init(obj->rtmp, &cb, obj->url);
//		if (NBRTMP_SetupURL(obj->rtmp,(char*)url) == FALSE){
//			ret = -1;
//			break;
//		}

		if(obj->is_publish){
			NBRTMP_EnableWrite(obj->rtmp);
		}

		NBRTMP_SetUserContext(obj->rtmp, obj);

		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
	//	sin.sin_family = AF_INET;
	//	sin.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */
	//	sin.sin_port = htons(port);

		NBRTMP_Get_Addr(obj->rtmp, &sin);
		obj->bev = bufferevent_socket_new(obj->ev_base, -1, BEV_OPT_CLOSE_ON_FREE);
		bufferevent_setcb(obj->bev, evrtmp_readcb, NULL, evrtmp_eventcb, obj);
	//	bufferevent_enable(obj->bev, EV_READ | EV_WRITE);
		if (bufferevent_socket_connect(obj->bev, (struct sockaddr *) &sin,
				(int)sizeof(sin)) < 0) {
			dbge("error start connect\n");
			ret = -1;
			break;
		}


		if(obj->evtimer){
			event_free(obj->evtimer);
			obj->evtimer = NULL;
		}
		struct timeval timeout;
		timeout.tv_sec = 3;
		timeout.tv_usec = 0;
		obj->evtimer = evtimer_new(obj->ev_base, conn_timeoutcb, obj);
		evtimer_add(obj->evtimer, &timeout);

		xnalu_spspps_reset(&obj->spspps);


	}while(0);

	if(ret < 0){
		evrtmp_client_close(obj);
	}

	return ret;
}


evrtmp_client_t * evrtmp_client_create(evrtmp_client_callback * cb, struct event_base * ev_base){
	evrtmp_client_t * obj = (evrtmp_client_t *) malloc(sizeof(struct evrtmp_client_s));
	memset(obj, 0, sizeof(struct evrtmp_client_s));
	if(cb){
		obj->cb = *cb;
	}
	obj->ev_base = ev_base;
	xnalu_spspps_init(&obj->spspps);
	return obj;
}

void evrtmp_client_delete(evrtmp_client_t * obj){
	if(!obj) return;
	evrtmp_client_close(obj);
	xnalu_spspps_close(&obj->spspps);
	if(obj->url){
		free(obj->url);
		obj->url = NULL;
	}
	free(obj);
}

void evrtmp_client_set_user_context(evrtmp_client_t * obj, void * user_context){
	obj->user_context = user_context;
}

void * evrtmp_client_get_user_context(evrtmp_client_t *obj){
	return obj->user_context;
}

void evrtmp_client_close(evrtmp_client_t * obj){
	if(obj->evtimer){
		event_free(obj->evtimer);
		obj->evtimer = NULL;
	}

	if(obj->bev){
		bufferevent_free(obj->bev);
		obj->bev = 0;
	}
	if(obj->rtmp){
		NBRTMP_Close(obj->rtmp);
		obj->rtmp = 0;
	}
}



int evrtmp_client_connect(evrtmp_client_t * obj, const char * url, int is_publish){
	if(obj->url) return 0;

	obj->is_publish = is_publish;

	int url_len = strlen(url);
	obj->url = (char *) malloc(url_len+1);
	memcpy(obj->url, url, url_len);
	obj->url[url_len] = '\0';

	obj->connect_try = 3;
	return kick_connect(obj);
}

int evrtmp_client_process_meta(evrtmp_client_t * obj
		, uint32_t timestamp, uint8_t * nalu, uint32_t nalu_offset, uint32_t nalu_len, uint32_t size){
//	size = nalu_len;

	int ret ;
	ret = xnalu_spspps_input(&obj->spspps, nalu+nalu_offset, nalu_len);
	if(ret <= 0) return ret;

	//		sps type=7, length=25
	//		sps 67 64 00 15 AC B2
	//		pps type=8, length=6
	//		pps 68 EB C3 CB 22 C0

	xsbuf_t * sps = xnalu_spspps_sps(&obj->spspps);
	xsbuf_t * pps = xnalu_spspps_pps(&obj->spspps);
	dbgd("SPS: type=%d, length=%d\n", sps->ptr[0] & 0x1f, sps->length);
	dbgd("PPS: type=%d, length=%d\n", pps->ptr[0] & 0x1f, pps->length);
	dbgi("send sps.len=%d, pps.len=%d\n", sps->length, pps->length);


	RTMPPacket* packet = &obj->packet4send;
	uint8_t * buf = 0;
	size_t need_size = RTMP_MAX_HEADER_SIZE + 16 + sps->length + pps->length;
	uint8_t * body ;
	if(size > need_size){
		body = nalu + RTMP_MAX_HEADER_SIZE;
//		dbgi("using user buffer\n");
	}else{
		buf = (uint8_t *) malloc(need_size);
		body = buf + RTMP_MAX_HEADER_SIZE;
//		dbgi("malloc buffer\n");
	}

	int i;
	memset(packet, 0, sizeof(RTMPPacket));
	packet->m_body = (char *) body;
	i = 0;
	body[i++] = 0x17;
	body[i++] = 0x00;

	body[i++] = 0x00;
	body[i++] = 0x00;
	body[i++] = 0x00;

	/*AVCDecoderConfigurationRecord*/
	body[i++] = 0x01;
	body[i++] = sps->ptr[1];
	body[i++] = sps->ptr[2];
	body[i++] = sps->ptr[3];
	body[i++] = 0xff;

	/*sps*/
	body[i++]   = 0xe1;
	body[i++] = (sps->length >> 8) & 0xff;
	body[i++] = sps->length & 0xff;
	memcpy(&body[i],sps->ptr, sps->length);
	i +=  sps->length;

	/*pps*/
	body[i++]   = 0x01;
	body[i++] = (pps->length >> 8) & 0xff;
	body[i++] = (pps->length) & 0xff;
	memcpy(&body[i], pps->ptr, pps->length);
	i +=  pps->length;

	packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
	packet->m_nBodySize = i;
	packet->m_nChannel = 0x04;
	packet->m_nTimeStamp = timestamp;
//	packet->m_hasAbsTimestamp = 0;
//	packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet->m_hasAbsTimestamp = 1;
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet->m_nInfoField2 = obj->rtmp->r.m_stream_id;


	int ok = NBRTMP_SendPacket(obj->rtmp, packet, TRUE);

	if(buf){
		free(buf);
//		dbgi("free buffer\n");
	}

	if (!ok) {
		return -1;
	}
	return 0;
}


// nalu_offset >= EVRTMP_NALU_OFFSET
int evrtmp_client_send_nalu(evrtmp_client_t * obj, uint32_t timestamp, uint8_t * buf, uint32_t nalu_offset, uint32_t nalu_len, uint32_t size){
	RTMPPacket* packet  = &obj->packet4send;
	uint8_t * body = buf + nalu_offset - EVRTMP_AVC_HEAD;

	if(nalu_offset < EVRTMP_NALU_OFFSET){
		return -1;
	}

	if(!obj->rtmp){
		return 0;
	}

	if(!NBRTMP_IsPlaying(obj->rtmp)){
		return 0;
	}

	memset(packet, 0, sizeof(RTMPPacket));
	memset(body, 0, EVRTMP_AVC_HEAD);
	int nal_type = buf[nalu_offset]&0x1f;
	int i = 0;

	obj->last_video_ts = timestamp;
	int32_t ts_diff = (int32_t)(obj->last_video_ts - obj->last_audio_ts);
	dbgd("nal_type=%d, len=%d, ts=%d (diff %d)\n", nal_type, nalu_len, timestamp, ts_diff);


	if(nal_type == NALU_TYPE_SPS){
		return evrtmp_client_process_meta(obj, timestamp, buf, nalu_offset, nalu_len, size);
	}else if(nal_type == NALU_TYPE_PPS){
		return evrtmp_client_process_meta(obj, timestamp, buf, nalu_offset, nalu_len, size);
	}else {
		if(!xnalu_spspps_exist(&obj->spspps)) return 0;

		// see E.4.3.1 VIDEODATA of video_file_format_spec_v10_1.pdf
		if (nal_type == NALU_TYPE_IDR) {
			body[i++] = 0x17;  // 1:Iframe  7:AVC
			body[i++] = 0x01;  // AVC NALU
			body[i++] = 0x00;
			body[i++] = 0x00;
			body[i++] = 0x00;

			// NALU size
			body[i++] = nalu_len >> 24 & 0xff;
			body[i++] = nalu_len >> 16 & 0xff;
			body[i++] = nalu_len >> 8 & 0xff;
			body[i++] = nalu_len & 0xff;
			// NALU data
//			memcpy(&body[i],data,nalu_len);
//			SendVideoSpsPps(rctx, metaData.Pps,metaData.nPpsLen,metaData.Sps,metaData.nSpsLen, nTimeStamp);
		} else {
			body[i++] = 0x27;  // 2:Pframe  7:AVC
			body[i++] = 0x01;  // AVC NALU
			body[i++] = 0x00;
			body[i++] = 0x00;
			body[i++] = 0x00;

			// NALU size
			body[i++] = nalu_len >> 24 & 0xff;
			body[i++] = nalu_len >> 16 & 0xff;
			body[i++] = nalu_len >> 8 & 0xff;
			body[i++] = nalu_len & 0xff;
			// NALU data
//			memcpy(&body[i],data,nalu_len);
		}
	}


	packet->m_nBodySize = EVRTMP_AVC_HEAD + nalu_len;
	packet->m_body = (char *) body;
	packet->m_hasAbsTimestamp = 0;
	packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
	packet->m_nInfoField2 = obj->rtmp->r.m_stream_id;
	packet->m_nChannel = 0x04;

	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
//	if (RTMP_PACKET_TYPE_AUDIO ==nPacketType && size !=4)
//	{
//		packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
//	}
	packet->m_nTimeStamp = timestamp;

	int ok =0;
	ok = NBRTMP_SendPacket(obj->rtmp, packet, TRUE);
//	return_packet(rctx, packet);
	if(!ok) {
		return -1;
	}
	return 0;
}


int evrtmp_client_send_speex(evrtmp_client_t * obj, uint32_t timestamp, uint8_t * buf, uint32_t offset, uint32_t length, uint32_t size){
	RTMPPacket* packet  = &obj->packet4send;
	uint8_t * body = buf + offset - EVRTMP_SPEEX_HEAD;

	if(offset < EVRTMP_SPEEX_OFFSET){
		return -1;
	}

	if(!obj->rtmp){
		return 0;
	}

	if(!NBRTMP_IsPlaying(obj->rtmp)){
		return 0;
	}

	obj->last_audio_ts = timestamp;
	int32_t ts_diff = (int32_t)(obj->last_audio_ts - obj->last_video_ts);
	dbgd("speex:  ts=%d (diff %d)\n", timestamp, ts_diff);

	memset(packet, 0, sizeof(RTMPPacket));

	// see  E.4.2.1 AUDIODATA  of video_file_format_spec_v10_1.pdf
	// "If the SoundFormat indicates Speex, the audio is compressed mono sampled at 16 kHz, 
	// the SoundRate shall be 0, the SoundSize shall be 1, and the SoundType shall be 0. "
	// so, body[0] should be 0xB2 ?
	body[0] = 0xB6; // SoundFormat=0xB, SoundRate=0x1, SoundSize=1, SoundType=0

//    RTMPPacket_Reset(&rtmp_pakt);
//    RTMPPacket_Alloc(&rtmp_pakt, buflen);
	packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
	packet->m_nBodySize = length + EVRTMP_SPEEX_HEAD;
	packet->m_body = (char *) body;
	packet->m_hasAbsTimestamp = 0;
	packet->m_nTimeStamp = timestamp;
	packet->m_nChannel = 4;
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet->m_nInfoField2 = obj->rtmp->r.m_stream_id;

	int ok =0;

	ok = NBRTMP_SendPacket(obj->rtmp, packet, TRUE);
//	return_packet(rctx, packet);
	if(!ok) {
		return -1;
	}
	return 0;
}

