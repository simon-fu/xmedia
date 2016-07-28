#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h> // SIGINT

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h> 
#include <netdb.h>


#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>


#include "util.h"
#include "xrtp_h264.h"

#define dbgv(...) do{  printf("<udp_dump>[D] " __VA_ARGS__); printf("\n"); }while(0)
#define dbgi(...) do{  printf("<udp_dump>[I] " __VA_ARGS__); printf("\n"); }while(0)
#define dbge(...) do{  printf("<udp_dump>[E] " __VA_ARGS__); printf("\n"); }while(0)

typedef struct tlv_file_st * tlv_file_t;


typedef struct udp_dumper_st *udp_dumper_t;

#define MEDIA_UNKNOWN 0
#define MEDIA_AUDIO 1
#define MEDIA_VIDEO 2

typedef struct udp_channel{
	struct udp_channel * next;
	udp_dumper_t owner;
	int sock;
	int port;
	struct event * ev_udp;
	long long packet_count;
	unsigned char buff[2*1024];
	int index;

	int media_type;
	struct sockaddr_in addrto;
	uint32_t dst_ssrc;
	uint32_t dst_payloadtype;
	int is_send_back;

	xrtp_to_nalu_t rtp2nalu;
	xnalu_to_rtp_t nalu2rtp;
	int nalu_buf_size;
	unsigned char * nalu_buf;
	int got_sps;
	int got_pps;
	int got_keyframe;
}udp_channel;

struct udp_dumper_st{
	struct event_base *		ev_base;
	struct event *			ev_signal;
	// FILE * fp;
	tlv_file_t tlvfile;
	udp_channel * channels;
};




struct tlv_file_st {
    FILE * fp;
    
};

int tlv_file_write(tlv_file_t obj, int type, int length, void * value){
    // unsigned short t16;
    // t16 = (unsigned short)type;
    fwrite(&type,    1, 4, obj->fp); // type
    fwrite(&length, 1, 4, obj->fp); // length
    fwrite(value,   1, length, obj->fp); // value
    return 0;
}

int tlv_file_write2(tlv_file_t obj, int type, int length1, void * value1, int length2, void * value2){
    // unsigned short t16;
    // t16 = (unsigned short)type;
    int length = length1 + length2;
    fwrite(&type,    1, 4, obj->fp); // type
    fwrite(&length, 1, 4, obj->fp); // length
    fwrite(value1,   1, length1, obj->fp); // value1
    fwrite(value2,   1, length2, obj->fp); // value2
    return 0;
}


void tlv_file_close(tlv_file_t obj){
    if(!obj) return;
    if(obj->fp){
        fclose(obj->fp);
        obj->fp = NULL;
    }
    free(obj);
}

tlv_file_t tlv_file_open(const char * filename){
    int ret = 0;
    tlv_file_t obj = NULL;
    do{
        obj = (tlv_file_t)malloc(sizeof(struct tlv_file_st));
        memset(obj, 0, sizeof(struct tlv_file_st));
        obj->fp = fopen(filename, "wb");
        if(!obj->fp){
            ret = -1;
            break;
        }
            
        ret = 0;
        
    }while(0);
    if(ret){
        tlv_file_close(obj);
        obj = NULL;
    }
    return obj;
}


static
int init_socket_addrin(const char * ip, int port, struct sockaddr_in * addr){
	memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if(ip){
    	addr->sin_addr.s_addr = inet_addr(ip);
    }else{
    	addr->sin_addr.s_addr = htonl(INADDR_ANY);
    }
    return 0;
}

static
const char * xnalu_type_string(int nalu_type){
	if(nalu_type == 5) return "(I)";
	if(nalu_type == 7) return "(SPS)";
	if(nalu_type == 8) return "(PPS)";
	return "";
}

static
void process_rtp(udp_channel * ch, int udp_len, struct sockaddr_in * from_addr){
	int ret = 0;
	if(ch->media_type == MEDIA_AUDIO){
		if(ch->dst_ssrc > 0){
			be_set_u32(ch->dst_ssrc, ch->buff+8);
		}
		ret = sendto(ch->sock, ch->buff, udp_len, 0, (struct sockaddr*)&ch->addrto, sizeof(ch->addrto));	
		ch->packet_count++;
	}else if(ch->media_type == MEDIA_VIDEO){
		ret = xrtp_to_nalu_next(&ch->rtp2nalu, ch->buff, udp_len);
		if(ret <= 0) return;
		int nalu_len = ret;

		int nal_type = ch->nalu_buf[0] & 0x1F;
		if(nal_type == 5 || nal_type == 7  || nal_type == 8 ){
			dbgi("nalu: type=%d%s, len=%d", nal_type, xnalu_type_string(nal_type), nalu_len);	
		}
		
		if(nal_type == 9) return;

		// if(nal_type == 7) {// sps
		// 	if(ch->got_sps) return;
		// 	ch->got_sps = 1;
		// }else if(nal_type == 8){ // pps
		// 	if(ch->got_pps) return;
		// 	ch->got_pps = 1;
		// }else if(nal_type == 5){
		// 	ch->got_keyframe = 1;
		// 	dbgi("got keyframe");
		// }else if(nal_type == 9){
		// 	return;
		// }else{
		// 	if(!ch->got_keyframe){
		// 		return;
		// 	}
		// }

		
		uint32_t timestamp = be_get_u32(ch->buff+4);
		ret = xnalu_to_rtp_first(&ch->nalu2rtp, timestamp, ch->nalu_buf, nalu_len, 1);
		if(ret == 0){
			int len;
			while ((len = xnalu_to_rtp_next(&ch->nalu2rtp, ch->buff)) > 0) {
				ret = sendto(ch->sock, ch->buff, len, 0, (struct sockaddr*)&ch->addrto, sizeof(ch->addrto));	
				ch->packet_count++;
			}
		}
	}
}

static
void on_udp_event(evutil_socket_t fd, short what, void *arg){
	// dbgv("on_udp_event");
	if(!(what&EV_READ)) return ;
	udp_channel * ch = (udp_channel *) arg;

    struct sockaddr_in tempadd;
    ev_socklen_t len = sizeof(tempadd);
    int bytes_to_recv = sizeof(ch->buff);
    
    while(1){
	    int sock_ret = recvfrom(ch->sock, ch->buff, bytes_to_recv, 0, (struct sockaddr*)&tempadd, &len);
	    if(sock_ret <= 0){
	    	// dbge("recvfrom fail with %d", sock_ret);
	    	return;
	    }
	    int udp_len = sock_ret;
	    // dbgv("port %d recv %d bytes", ch->port, udp_len);

		if(ch->owner->tlvfile){
			int64_t ts = get_timestamp_ms();
			tlv_file_write2(ch->owner->tlvfile, ch->index, sizeof(ts), &ts, udp_len, ch->buff);
			
		}

	 //    const char *from_ip = inet_ntoa(tempadd.sin_addr);
		// int from_port = ntohs(tempadd.sin_port);
	 //    dbgv("port %d recv %d bytes from %s:%d", ch->port, udp_len, from_ip, from_port);


		if(ch->is_send_back){
			// send back
			if(ch->dst_ssrc > 0){
				be_set_u32(ch->dst_ssrc, ch->buff+8);
			}
			sock_ret = sendto(ch->sock, ch->buff, udp_len, 0, (struct sockaddr*)&tempadd, sizeof(tempadd));
			// dbgi("sendto back ret %d", sock_ret);
			ch->packet_count++;
		}else{
			process_rtp(ch, udp_len, &tempadd);
		}
    }


	
}

static 
void destroy_channel(udp_channel * ch){
	if(!ch) return;

	if(ch->ev_udp){
		event_free(ch->ev_udp);
		ch->ev_udp = 0;
	}

	if(ch->sock>0){
		evutil_closesocket(ch->sock);
		ch->sock = 0;
	}

	free(ch);
}

static 
udp_channel * create_and_add_udp_channel(udp_dumper_t obj, int port){
	udp_channel * ch = NULL;
	int ret = 0;
	do{
		if(port > 65535){
			dbge("error port %d", port);
			ret = -1;
			break;
		}

		ZERO_ALLOC(ch, udp_channel *, sizeof(udp_channel));
		ch->port = port;
		ch->sock = create_udp_socket(port);
		if(ch->sock < 0){
			dbge("fail to create udp socket at port %d", port);
			ret = -1;
			break;
		}
		evutil_make_socket_nonblocking(ch->sock);
		ch->owner = obj;
		ch->ev_udp = event_new(obj->ev_base, ch->sock, EV_READ|EV_PERSIST, on_udp_event, ch);

		// insert head
		ch->next = obj->channels;
		obj->channels =ch;

		

		event_add(ch->ev_udp, NULL);
	}while(0);

	if(ret){
		destroy_channel(ch);
		ch = NULL;
	}

	return ch;
}

static 
void ev_signal_cb(evutil_socket_t sig, short events, void *user_data)
{
	udp_dumper_t obj = (udp_dumper_t) user_data;

	long seconds = 0;
	struct timeval delay = { seconds, 0 };

	// dbgi("*** caught signal (0x%08X) ***, stop after %ld seconds...\n", events, seconds);
	dbgi("stopping...");

	event_base_loopexit(obj->ev_base, &delay);
}


#define UDP_FILE_MAGIC  		"UDPv1"
#define UDP_FILE_MAGIC_SIZE		6

int main(int argc, char** argv){
	// if(argc <= 2){
	// 	dbgi("usage: %s -srcvport port1 -srcaport port2 -dstip ip", argv[0]);
	// 	return -1;
	// }

	int src_audio_port = 10000;
	int src_video_port = 0;

	// //const char * dst_ip = "172.17.3.161"; // note3
	// // const char * dst_ip = "172.17.1.247"; // huawei
	// const char * dst_ip = "127.0.0.1";
	// int dst_audio_port = 10000;
	// int dst_video_port = 10002;
	// uint32_t dst_ssrc = 0x1234;
	// uint32_t dst_video_payloadtype = 96;

	const char * dst_ip = "172.17.3.7"; // raoshangrong kurento
	int dst_audio_port = 35732;
	int dst_video_port = 0;
	uint32_t dst_ssrc = 0; //0x1234;
	uint32_t dst_video_payloadtype = 96;

	int is_send_back = 0;

	udp_dumper_t obj = NULL;


	const char * filename = argv[1];
	int portcount = 2;


	int ret = 0;

	udp_channel * video_ch = NULL;
	udp_channel * audio_ch = NULL;

	if(!is_send_back){
		if(src_audio_port > 0){
			dbgi("audio: %d -> %s:%d", src_audio_port, dst_ip, dst_audio_port);
		}
		
		if(src_video_port > 0){
			dbgi("video: %d -> %s:%d", src_video_port, dst_ip, dst_video_port);
		}
	}else{
		if(src_audio_port > 0){
			dbgi("audio: %d -> back", src_audio_port);
		}
		
		if(src_video_port > 0){
			dbgi("video: %d -> back", src_video_port);
		}
	}

	if(dst_ssrc > 0){
		dbgi("dst_ssrc = %d", dst_ssrc);
	}



	do{
		ZERO_ALLOC(obj, udp_dumper_t, sizeof(struct udp_dumper_st));
		if(filename){
			obj->tlvfile = tlv_file_open(filename);
			if(!obj->tlvfile){
				dbge("fail to writing open %s", filename);
				ret = -1;
				break;
			}
			dbgi("successfully writing open %s", filename);
		}


		obj->ev_base = event_base_new();
		obj->ev_signal = evsignal_new(obj->ev_base, SIGINT, ev_signal_cb, (void *)obj);
		event_add(obj->ev_signal, NULL);

		if(src_audio_port > 0){
			audio_ch = create_and_add_udp_channel(obj, src_audio_port);
			init_socket_addrin(dst_ip, dst_audio_port, &audio_ch->addrto);
			audio_ch->media_type = MEDIA_AUDIO;
			audio_ch->dst_ssrc = dst_ssrc;
			audio_ch->is_send_back = is_send_back;
		}
		
		if(src_video_port > 0){
			video_ch = create_and_add_udp_channel(obj, src_video_port);
			init_socket_addrin(dst_ip, dst_video_port, &video_ch->addrto);
			video_ch->media_type = MEDIA_VIDEO;
			video_ch->dst_ssrc = dst_ssrc;
			video_ch->is_send_back = is_send_back;

			video_ch->nalu_buf_size = 1*1024*1024;
			video_ch->nalu_buf = (unsigned char *) malloc(video_ch->nalu_buf_size);
			xrtp_to_nalu_init(&video_ch->rtp2nalu, video_ch->nalu_buf, video_ch->nalu_buf_size, 0);
			xnalu_to_rtp_init(&video_ch->nalu2rtp, dst_ssrc, dst_video_payloadtype, 512);
		}




		if(obj->tlvfile){
			unsigned char buf[16];

			tlv_file_write(obj->tlvfile, TLV_TYPE_MAGIC, UDP_FILE_MAGIC_SIZE, (void *)UDP_FILE_MAGIC);

			le_set_u32(portcount, buf);
			tlv_file_write(obj->tlvfile, TLV_TYPE_TYPECOUNT, 4, buf);

			udp_channel * ch = obj->channels;
			while(ch){
				le_set_u32(ch->index, buf+0);
				le_set_u32(ch->port,  buf+4);
				tlv_file_write(obj->tlvfile, TLV_TYPE_TYPEINFO, 8, buf);
				ch = ch->next;
			}
		}

		// loop 
		dbgi("start relaying...");
		event_base_dispatch(obj->ev_base);
		ret = 0;
	}while(0);

	if(ret == 0){
		udp_channel * ch = obj->channels;
		while(ch){
			dbgi("port %d relay %lld packets", ch->port, ch->packet_count);
			ch = ch->next;
		}
	}

	if(obj->tlvfile){
		tlv_file_close(obj->tlvfile);
		obj->tlvfile = NULL;
	}

	if(video_ch){
		if(video_ch->nalu_buf){
			free(video_ch->nalu_buf);
			video_ch->nalu_buf = NULL;
		}
	}

	if(obj->channels){
		udp_channel * ch = obj->channels;
		while(ch){
			udp_channel * next = ch->next;
			destroy_channel(ch);
			ch = next;
		}
		obj->channels = NULL;
	}

	if(obj->ev_signal){
		event_free(obj->ev_signal);
		obj->ev_signal = 0;
	}

	if(obj->ev_base){
		event_base_free(obj->ev_base);
		obj->ev_base = 0;
	}

	free(obj);

	
	return 0;
}

