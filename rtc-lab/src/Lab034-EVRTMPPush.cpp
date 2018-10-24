
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <signal.h> // SIGINT
#include <limits.h>
#include <list>
#include <map>
#include <unordered_map>


#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include <sys/socket.h>
#include <netinet/tcp.h>
#ifndef WIN32
#include <arpa/inet.h> // inet_ntoa
#endif



#include "Lab034-EVRTMPPush.hpp"
#include "libevrtmp/amf.h"
#include "libevrtmp/rtmp.h"
#include "xrtp_h264.h"
#include "evrtmp_client.h"
#include "xnalu.h"
#include "xutil.h"


#define MNAME "<evrtmp> "
#include "dbg_defs.h"

#undef dbgd
#define dbgd(...)


static uint8_t annex[] = {0x00, 0x00, 0x01};
static uint32_t annex_len = sizeof(annex);

typedef struct evmain_ctx_s evmain_ctx;
struct evmain_ctx_s{
    NBRTMP * rtmp;
    FILE * fp_rtmp_video;
    FILE * fp_rtmp_audio;
    // FILE * fp_rtp_video;
    // FILE * fp_rtp_audio;
    
    struct event_base * ev_base;
    struct event *evtimer;
    int timer_active;
    
    evutil_socket_t            sock_udp;
    uint8_t udp_data[1600];
    xrtp_to_nalu_t rtp2nalu;
    uint8_t * nalu;
    unsigned nalu_size;
    
    
    uint32_t audio_last_ts32;
    uint64_t audio_last_ts64;
    
    xnalu_spspps_t rtmp_spspps;
    
    struct sockaddr_in remote_addr;
    struct sockaddr_in remote_addr_audio;
    uint32_t audio_seq;
    
    evrtmp_client_t * push_client;
    evrtmp_client_t * pull_client;
    
    int64_t last_pts_video;
    int64_t last_pts_audio;
    
    uint8_t * raw_video_data;
    size_t raw_video_size;
    size_t raw_video_pos;
    size_t raw_video_len;
    uint32_t push_frame_num;
    int64_t next_push_video_time;
};

static int
write_file(FILE * fp, const void * data, unsigned length)
{
    const uint8_t * p = (const uint8_t *)data;
    int remains = length;
    while (remains > 0) {
        int bytes = fwrite(p, 1, remains, fp);
        if (bytes < 0) {
            printf("write file fail!!!\n");
            return -1;
        }
        p += bytes;
        remains -= bytes;
    }
    fflush(fp);
    return 0;
}



static
int found_nalu_annex(uint8_t * data, size_t data_size, size_t& pos, size_t& nalu_pos){
    int found_annex = 0;
    size_t last_check_pos = (data_size-4);
    for(;pos < last_check_pos; ++pos){
        if(data[pos+0] == 0 && data[pos+1] == 0){
            if(data[pos+2] == 1){
                nalu_pos = pos+3;
                found_annex = 1;
                break;
            }else if(data[pos+2] == 0 && data[pos+3] == 1){
                nalu_pos = pos+4;
                found_annex = 1;
                break;
            }
        }
    }
    if(!found_annex){
        if(pos == last_check_pos){
            ++pos;
            if(data[pos+0] == 0 && data[pos+1] == 0 && data[pos+2] == 1){
                nalu_pos = pos+3;
                found_annex = 1;
                return found_annex;
            }
        }
        pos = data_size;
        nalu_pos = pos;
    }
    return found_annex;
}

static
int read_h264_next_nalu(uint8_t * data, size_t data_size, size_t& pos, size_t &length, size_t& nalu_pos){
    
    pos += length;
    if(pos >= data_size){
        return -1;
    }
    
    size_t start_pos = pos;
    size_t next_nalu_pos = nalu_pos;
    int found_annex = found_nalu_annex(data, data_size, pos, next_nalu_pos);
    if(!found_annex){
        return -2;
    }
    
    start_pos = pos;
    nalu_pos = next_nalu_pos;
    pos = next_nalu_pos;
    found_annex = found_nalu_annex(data, data_size, pos, next_nalu_pos);
    length = pos - nalu_pos;
    
    pos = start_pos;
    return 0;
}

static
void dump_video_buf(uint8_t *raw_video_data, size_t raw_video_size){
    size_t raw_video_pos = 0;
    size_t raw_video_len = 0;
    int ret = 0;
    while(1){
        size_t nalu_pos = raw_video_pos;
        ret = read_h264_next_nalu(raw_video_data, raw_video_size, raw_video_pos, raw_video_len, nalu_pos);
        if(ret < 0){
            break;
        }
        dbgi("nalu: pos=%ld, nalu=%ld, len=%ld, end=%ld", raw_video_pos, nalu_pos, raw_video_len, nalu_pos+raw_video_len);
    }
}

static
int load_video_file(const char * fname, uint8_t **buf, size_t * buf_size){
    FILE * fp_rtmp_video = NULL;
    uint8_t * raw_video_data = NULL;
    size_t raw_video_size = 0;
    
    int ret = 0;
    do{
        fp_rtmp_video = fopen(fname, "rb");
        if(!fp_rtmp_video){
            dbge("fail to open video file [%s]", fname);
            break;
        }
        ret = fseek(fp_rtmp_video, 0, SEEK_END);
        raw_video_size = ftell(fp_rtmp_video);
        ret = fseek(fp_rtmp_video, 0, SEEK_SET);
        dbgi("video file size %ld", raw_video_size);
        if(raw_video_size <= 0){
            dbge("invalid video file size");
            ret = -1;
            break;
        }
        raw_video_data = (uint8_t *) malloc(raw_video_size);
        long bytes = fread(raw_video_data, 1, raw_video_size, fp_rtmp_video);
        if(bytes != raw_video_size){
            dbge("read video file fail, %ld", bytes);
            ret = -1;
            break;
        }
        
        // dump_video_buf(raw_video_data, raw_video_size);
        
        if(buf){
            *buf = raw_video_data;
            raw_video_data = NULL;
        }
        if(buf_size){
            *buf_size = raw_video_size;
            raw_video_size = 0;
        }
        ret = 0;
        
    }while(0);
    if(fp_rtmp_video){
        fclose(fp_rtmp_video);
        fp_rtmp_video = NULL;
    }
    if(raw_video_data){
        free(raw_video_data);
        raw_video_data = NULL;
    }
    return ret;
}


static int kick_timer(evmain_ctx * rctx, int64_t next_time_ms){
    if(!rctx->timer_active){
        int64_t interval_ms = next_time_ms - xmstime();
        if(interval_ms < 0){
            interval_ms = 0;
        }
        dbgi("kick_timer: interval_ms = %lld", interval_ms);
        rctx->timer_active = 1;
        struct timeval timeout;
        timeout.tv_sec = interval_ms/1000;
        timeout.tv_usec = (interval_ms%1000)*1000;
        evtimer_add(rctx->evtimer, &timeout);
        return 0;
    }else{
        return -1;
    }
}

static void timeoutcb(evutil_socket_t fd, short what, void *arg) {
    evmain_ctx * rctx = (evmain_ctx *) arg;
    //dbgi("timeout");
    rctx->timer_active = 0;
    
    int ret = 0;
    size_t nalu_pos = 0;
    ret = read_h264_next_nalu(rctx->raw_video_data, rctx->raw_video_size, rctx->raw_video_pos, rctx->raw_video_len, nalu_pos);
    if(ret < 0 && rctx->push_frame_num > 0){
        dbgi("reach file end, loop");
        rctx->raw_video_pos = 0;
        rctx->raw_video_len =0;
        ret = read_h264_next_nalu(rctx->raw_video_data, rctx->raw_video_size, rctx->raw_video_pos, rctx->raw_video_len, nalu_pos);
    }
    
    if(ret < 0){
        dbgi("reach file end, exit");
        event_base_loopexit(rctx->ev_base, NULL);
    }else{
        uint32_t interval_ms = 40;
        uint32_t pts = rctx->push_frame_num * interval_ms;
        ++rctx->push_frame_num;
        dbgi("No.%u nalu: pts=%u, pos=%ld, nalu=%ld, len=%ld, end=%ld", rctx->push_frame_num, pts, rctx->raw_video_pos, nalu_pos, rctx->raw_video_len, nalu_pos+rctx->raw_video_len);
        memcpy(rctx->nalu+EVRTMP_NALU_OFFSET, &rctx->raw_video_data[nalu_pos], rctx->raw_video_len);
        evrtmp_client_send_nalu(rctx->push_client, (uint32_t) pts
                                , rctx->nalu, EVRTMP_NALU_OFFSET, rctx->raw_video_len, rctx->nalu_size);
        rctx->next_push_video_time += interval_ms;
        kick_timer(rctx, rctx->next_push_video_time);
    }
    
    
    //  event_base_loopexit(base, NULL);
}



static
int on_recv_rtmp_nalu(evrtmp_client_t *e, uint32_t pts, const uint8_t * nalu, uint32_t nalu_len){
    evmain_ctx * rctx = (evmain_ctx *) evrtmp_client_get_user_context(e);
    
    if(nalu_len < 1) return 0;
    int64_t pts64 = pts ;
    int delta = (int) (pts64 - rctx->last_pts_video);
    rctx->last_pts_video = pts;
    const char * sign = delta > 0 ? "+":"";
    int diff = (int) (rctx->last_pts_video - rctx->last_pts_audio);
    const char * diffsign = diff > 0 ? "+":"";
    
    dbgd("on_recv_rtmp_nalu: type=%d, len=%d, pts=%d (%s%d), diff=%s%d", nalu[0]&0x1F, nalu_len, pts, sign, delta, diffsign, diff);
    if (delta > 150 || delta < 0) {
        dbgi("video =======================================>>>>>>>>");
    }
    
    
    // if (rctx->fp_rtmp_video) {
    //     write_file(rctx->fp_rtmp_video, annex, annex_len);
    //     write_file(rctx->fp_rtmp_video, nalu, nalu_len);
    // }
    
    xsbuf_t * sps = xnalu_spspps_sps(&rctx->rtmp_spspps);
    xsbuf_t * pps = xnalu_spspps_pps(&rctx->rtmp_spspps);
    if(xnalu_spspps_input(&rctx->rtmp_spspps, nalu, nalu_len) > 0){
        dbgi("got rtmp sps.len=%d, pps.len=%d", sps->length, pps->length);
    }
    //    if(xnalu_spspps_exist(&rctx->rtmp_spspps)){
    //        if(xnalu_type(nalu) == NALU_TYPE_IDR){
    //            dbgi("got rtmp IDR");
    //            send_nalu2rtp(rctx, pts, sps->ptr, sps->length);
    //            send_nalu2rtp(rctx, pts, pps->ptr, pps->length);
    //        }
    //        send_nalu2rtp(rctx, pts, nalu, nalu_len);
    //    }
    
    return 0;
}


static
int on_recv_rtmp_speex(evrtmp_client_t *e, uint32_t pts, const uint8_t * data, uint32_t data_len){
    evmain_ctx * rctx = (evmain_ctx *) evrtmp_client_get_user_context(e);
    
    
    int64_t pts64 = pts ;
    int delta = (int) (pts64 - rctx->last_pts_audio);
    rctx->last_pts_audio = pts;
    const char * sign = delta > 0 ? "+":"";
    int diff = (int) (rctx->last_pts_audio - rctx->last_pts_video);
    const char * diffsign = diff > 0 ? "+":"";
    
    dbgi("on_recv_rtmp_speex: len=%d, pts=%d (%s%d), diff=%s%d", data_len, pts, sign, delta, diffsign, diff);
    if(delta > 40 || delta < 0){
        dbgi("audio =======================================>>>>>>>>");
    }
    
    if(data_len < 1) return 0;
    return 0;
}

int lab034_evrtmp_push_main(int argc, char** argv){
    
    // const char * pull_url = "rtmp://121.41.87.159/myapp/videosr-aaa'user1";
    // const char * pull_url = "rtmp://localhost/myapp/videosr-simon$simon2";
    //const char * push_url = "rtmp://localhost/myapp/test#u1";
    const char * push_url = "rtmp://localhost/myapp/raw";
    
    //const char * fname_rtmp_video = "/tmp/rtmp.h264";
    const char * fname_rtmp_video = "/Users/simon/easemob/src/study/rtmp/srs-librtmp/src/srs/720p.h264.raw";
    const char * fname_rtmp_audio = "/tmp/rtmp.pcm";
    int is_publish = 1;
    
    
    int port = 1935;
    
    if(argc > 1){
        push_url = argv[1];
    }
    
    if(argc > 2){
        fname_rtmp_video = argv[2];
    }
    
    if(argc > 3) {
        fname_rtmp_audio = argv[3];
    }
    
    dbgi("push_url=%s\n", push_url);
    dbgi("fname_rtmp_video=%s\n", fname_rtmp_video);
    dbgi("fname_rtmp_audio=%s\n", fname_rtmp_audio);
    
    int ret = 0;
    struct event_base * ev_base = 0;
    struct event *evtimeout;
    evmain_ctx * rctx = (evmain_ctx *) malloc(sizeof(evmain_ctx));
    memset(rctx, 0, sizeof(evmain_ctx));
    xnalu_spspps_init(&rctx->rtmp_spspps);
    
    do{
        ret = load_video_file(fname_rtmp_video, &rctx->raw_video_data, &rctx->raw_video_size);
        if(ret){
            dbge("fail to load video file [%s]", fname_rtmp_video);
            break;
        }
        //dump_video_buf(rctx->raw_video_data, rctx->raw_video_size);
        rctx->raw_video_pos = 0;
        
        // while(1){
        //     size_t nalu_pos = 0;
        //     ret = read_h264_next_nalu(rctx->raw_video_data, rctx->raw_video_size, rctx->raw_video_pos, rctx->raw_video_len, nalu_pos);
        //     if(ret < 0){
        //         break;
        //     }
        //     dbgi("nalu: pos=%ld, nalu=%ld, len=%ld, end=%ld", rctx->raw_video_pos, nalu_pos, rctx->raw_video_len, nalu_pos+rctx->raw_video_len);
        // }
        
        
        // rctx->fp_rtmp_audio = fopen(fname_rtmp_audio, "wb");
        
        ev_base = event_base_new();
        if (!ev_base) {
            dbge("Couldn't open event base");
            break;
        }
        rctx->ev_base = ev_base;
        
        evrtmp_client_callback cb = {
            on_recv_rtmp_nalu,
            on_recv_rtmp_speex
        };
        rctx->push_client = evrtmp_client_create(&cb, ev_base);
        evrtmp_client_set_user_context(rctx->push_client, rctx);
        evrtmp_client_connect(rctx->push_client, push_url, is_publish);
        
        
        rctx->next_push_video_time = xmstime();
        evtimeout = evtimer_new(ev_base, timeoutcb, rctx);
        rctx->evtimer = evtimeout;
        kick_timer(rctx, rctx->next_push_video_time);
        
        rctx->nalu_size = 128*1024;
        rctx->nalu = (uint8_t *) malloc(rctx->nalu_size);
        //    xrtp_to_nalu_init(&rctx->rtp2nalu, rctx->nalu + EVRTMP_NALU_OFFSET, rctx->nalu_size-EVRTMP_NALU_OFFSET);
        
        event_base_dispatch (ev_base);
        
    }while(0);
    
    if(rctx->push_client){
        evrtmp_client_delete(rctx->push_client);
        rctx->push_client = NULL;
    }
    event_free(evtimeout);
    event_base_free(ev_base);
    xnalu_spspps_close(&rctx->rtmp_spspps);
    if(rctx->fp_rtmp_video){
        fclose(rctx->fp_rtmp_video);
        rctx->fp_rtmp_video = NULL;
    }
    if(rctx->fp_rtmp_audio){
        fclose(rctx->fp_rtmp_audio);
        rctx->fp_rtmp_audio = NULL;
    }
    if(rctx->raw_video_data){
        free(rctx->raw_video_data);
        rctx->raw_video_data = NULL;
    }
    if(rctx->nalu){
        free(rctx->nalu);
        rctx->nalu = NULL;
    }
    free(rctx);
    
    printf("bye\n");
    return 0;
}




