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


#include "xcutil.h"
#include "xrtp_h264.h"

#define dbgv(...) do{  printf("<udp_dump>[D] " __VA_ARGS__); printf("\n"); }while(0)
#define dbgi(...) do{  printf("<udp_dump>[I] " __VA_ARGS__); printf("\n"); }while(0)
#define dbge(...) do{  printf("<udp_dump>[E] " __VA_ARGS__); printf("\n"); }while(0)

typedef struct tlv_file_st * tlv_file_t;


typedef struct udp_dumper_st *udp_dumper_t;

#define MEDIA_UNKNOWN 0
#define MEDIA_AUDIO 1
#define MEDIA_VIDEO 2

#define DIR_SRC_TO_DST 0x1
#define DIR_DST_TO_SRC 0x2
#define DIR_BOTH (DIR_SRC_TO_DST | DIR_DST_TO_SRC)

typedef struct peer_channel{
	char ip[64];
	int port;
	struct sockaddr_in addr;

	int recv_first;
	long long packet_count;

	int nalu_buf_size;
	unsigned char * nalu_buf;
	union{
		xrtp_h264_repacker video_repacker;
		xrtp_transformer audio_transformer;
	};
}peer_channel;

typedef struct udp_channel{
	struct udp_channel * next;
	udp_dumper_t owner;
	int sock;
	int local_port;
	// struct sockaddr_in addrto;
	struct event * ev_udp;
	
	unsigned char buff[2*1024];
	int index;

	int media_type;
	char name[64];
	int is_send_back;
	uint32_t local_ssrc;
	uint32_t direction;

	int addr_done;
	peer_channel src;
	peer_channel dst;


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
    if(ip && ip[0] != '\0'){
    	addr->sin_addr.s_addr = inet_addr(ip);
    }else{
    	addr->sin_addr.s_addr = htonl(INADDR_ANY);
    }
    return 0;
}

static
int sock_addr_is_valid(struct sockaddr_in * src){
	return (src->sin_family > 0
		&& src->sin_addr.s_addr > 0
		&& src->sin_port > 0);
}

static
int sock_addr_is_equ(struct sockaddr_in * src, struct sockaddr_in * dst){
	return (src->sin_family == dst->sin_family
		&& src->sin_addr.s_addr == dst->sin_addr.s_addr
		&& src->sin_port == dst->sin_port);
}




static
void send_directly(udp_channel * ch, peer_channel * peer, unsigned char * data, int data_len){
	int ret;
	ret = sendto(ch->sock, data, data_len, 0, (struct sockaddr*)&peer->addr, sizeof(peer->addr));
	peer->packet_count++;
}

static
void process_audio_rtp(udp_channel * ch, peer_channel * peer_from, peer_channel * peer_to, unsigned char * data, int data_len){
	xrtp_transformer_process(&peer_from->audio_transformer, data);
	send_directly(ch, peer_to, data, data_len);
}

static
void process_video_rtp(udp_channel * ch, peer_channel * peer_from, peer_channel * peer_to, unsigned char * data, int data_len){
	int ret = xrtp_h264_repacker_input(&peer_from->video_repacker, data, data_len);
	if(ret > 0){
		int len;
		while ((len = xrtp_h264_repacker_next(&peer_from->video_repacker, data)) > 0) {
			// dump_rtp("video rtp2", data, len);
			send_directly(ch, peer_to, data, len);
			// ret = sendto(ch->sock, data, len, 0, (struct sockaddr*)&ch->addrto, sizeof(ch->addrto));	
			// ch->packet_count++;
		}
	}
}

static
void process_rtp(udp_channel * ch, unsigned char * data, int data_len, struct sockaddr_in * from_addr){

	peer_channel * peer_from;
	peer_channel * peer_to;
	if(sock_addr_is_equ(from_addr, &ch->src.addr)){
		peer_from = &ch->src;
		peer_to = &ch->dst;
		if(!(ch->direction & DIR_SRC_TO_DST)){
			return;
		}
	}else if(sock_addr_is_equ(from_addr, &ch->dst.addr)){
		peer_from = &ch->dst;
		peer_to = &ch->src;
		if(!(ch->direction&DIR_DST_TO_SRC)){
			return;
		}
	}else{
		return;
	}

	// int ret = 0;
	if(ch->media_type == MEDIA_AUDIO){
		process_audio_rtp(ch, peer_from, peer_to, data, data_len);
	}else if(ch->media_type == MEDIA_VIDEO){
		process_video_rtp(ch, peer_from, peer_to, data, data_len);
	}
}

static 
bool check_set_peer_addr(udp_channel * ch, peer_channel * peer, struct sockaddr_in * from , const char * direction){
	if(peer->addr.sin_addr.s_addr > 0){
		if(peer->addr.sin_addr.s_addr != from->sin_addr.s_addr){
			return false;
		}
	}

	if(peer->addr.sin_port > 0){
		if(peer->addr.sin_port != from->sin_port){
			return false;
		}
	}

	if(!sock_addr_is_valid(&peer->addr) || !peer->recv_first){
		peer->recv_first = 1;
		peer->addr = *from;
		const char *from_ip = inet_ntoa(from->sin_addr);
		int from_port = ntohs(from->sin_port);
		dbgi("%s got %s %s:%d", ch->name, direction, from_ip, from_port);
	}

	return true;
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
	    // dbgi("port %d recv %d bytes", ch->local_port, udp_len);

		if(ch->owner->tlvfile){
			int64_t ts = get_timestamp_ms();
			tlv_file_write2(ch->owner->tlvfile, ch->index, sizeof(ts), &ts, udp_len, ch->buff);
			
		}

	 //    const char *from_ip = inet_ntoa(tempadd.sin_addr);
		// int from_port = ntohs(tempadd.sin_port);
	 //    dbgv("port %d recv %d bytes from %s:%d", ch->local_port, udp_len, from_ip, from_port);

		// send back
		if(ch->is_send_back){
			if(ch->local_ssrc > 0){
				be_set_u32(ch->local_ssrc, ch->buff+8);
			}
			sock_ret = sendto(ch->sock, ch->buff, udp_len, 0, (struct sockaddr*)&tempadd, sizeof(tempadd));
			// dbgi("sendto back ret %d", sock_ret);
			// ch->packet_count++;
			return;
		}

		// check src and dst
		if(!ch->addr_done){
			if(!check_set_peer_addr(ch, &ch->src, &tempadd, "src")){
				check_set_peer_addr(ch, &ch->dst, &tempadd, "dst");
			}

			if(sock_addr_is_valid(&ch->src.addr) && sock_addr_is_valid(&ch->dst.addr)){
				dbgi("%s check src and dst done", ch->name);
				ch->addr_done = 1;
			}
		}
		// dbgi("ch->addr_done %d", ch->addr_done);
		if(ch->addr_done){
			process_rtp(ch, ch->buff, udp_len, &tempadd);
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
		ch->local_port = port;
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

	int is_send_back = 0;
	int direction = DIR_BOTH; // DIR_BOTH  DIR_SRC_TO_DST DIR_DST_TO_SRC


	int local_audio_port = 10000;
	int local_video_port = 10002;

	const char * src_ip = "172.17.3.161"; // note3
	int src_audio_port = 10000;
	int src_video_port = 10002;

	// //const char * dst_ip = "172.17.3.161"; // note3
	// // const char * dst_ip = "172.17.1.247"; // huawei
	// const char * dst_ip = "172.17.3.7"; // raoshangrong kurento
	// const char * dst_ip = "127.0.0.1";
	const char * dst_ip = "";
	int dst_audio_port = 0;
	int dst_video_port = 0;


	uint32_t local_ssrc = 0x1234;
	uint32_t local_audio_payloadtype = 120;
	uint32_t local_video_payloadtype = 96;

	#if 1
	// voice -> rtc
	int src_samplerate = 16000;
	int dst_samplerate = 48000;
	#else
	// rtc -> voice
	int src_samplerate = 48000;
	int dst_samplerate = 16000;

	#endif


	udp_dumper_t obj = NULL;


	const char * filename = argv[1];
	int portcount = 2;


	int ret = 0;

	udp_channel * video_ch = NULL;
	udp_channel * audio_ch = NULL;

	const char * dir ;
	if(direction == DIR_SRC_TO_DST){
		dir = "->";
	}else if(direction == DIR_DST_TO_SRC){
		dir = "<-";
	}else if(direction == DIR_BOTH){
		dir = "<->";
	}else{
		dbge("unknown direction %02X", direction);
		return -1;
	}

	if(!is_send_back){
		dbgi("local_ssrc = %d", local_ssrc);
		dbgi("local_audio_payloadtype = %d", local_audio_payloadtype);
		dbgi("local_video_payloadtype = %d", local_video_payloadtype);
		dbgi("src_samplerate = %d", src_samplerate);
		dbgi("dst_samplerate = %d", dst_samplerate);


		if(local_audio_port > 0){
			dbgi("audio: src %s:%d %s local %d %s dst %s:%d", src_ip, src_audio_port, dir, local_audio_port, dir, dst_ip, dst_audio_port);
		}
		if(local_video_port > 0){
			dbgi("video: src %s:%d %s local %d %s dst %s:%d", src_ip, src_video_port, dir, local_video_port, dir, dst_ip, dst_video_port);
		}

	}else{
		if(local_audio_port > 0){
			dbgi("audio: %d -> back", local_audio_port);
		}
		
		if(local_video_port > 0){
			dbgi("video: %d -> back", local_video_port);
		}
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

		if(local_audio_port > 0){
			audio_ch = create_and_add_udp_channel(obj, local_audio_port);
			udp_channel * ch = audio_ch;
			ch->is_send_back = is_send_back;
			ch->media_type = MEDIA_AUDIO;
			ch->local_ssrc = local_ssrc;
			ch->direction = direction;
			sprintf(ch->name, "audio");
			init_socket_addrin(src_ip, src_audio_port, &ch->src.addr);
			init_socket_addrin(dst_ip, dst_audio_port, &ch->dst.addr);

			xrtp_transformer_init(&ch->src.audio_transformer, src_samplerate, dst_samplerate, local_audio_payloadtype, local_ssrc );
			xrtp_transformer_init(&ch->dst.audio_transformer, dst_samplerate, src_samplerate, local_audio_payloadtype, local_ssrc );
		}
		
		if(local_video_port > 0){
			video_ch = create_and_add_udp_channel(obj, local_video_port);
			udp_channel * ch = video_ch;
			ch->is_send_back = is_send_back;
			ch->media_type = MEDIA_VIDEO;
			ch->local_ssrc = local_ssrc;
			ch->direction = direction;
			sprintf(ch->name, "video");
			init_socket_addrin(src_ip, src_video_port, &ch->src.addr);
			init_socket_addrin(dst_ip, dst_video_port, &ch->dst.addr);

			ch->src.nalu_buf_size = 1*1024*1024;
			ch->src.nalu_buf = (unsigned char *) malloc(ch->src.nalu_buf_size);
			xrtp_h264_repacker_init(&ch->src.video_repacker
					, ch->src.nalu_buf, ch->src.nalu_buf_size, 0
					, local_ssrc, local_video_payloadtype, 1412
					, 0, 0);

			ch->dst.nalu_buf_size = 1*1024*1024;
			ch->dst.nalu_buf = (unsigned char *) malloc(ch->dst.nalu_buf_size);
			xrtp_h264_repacker_init(&ch->dst.video_repacker
					, ch->dst.nalu_buf, ch->dst.nalu_buf_size, 0
					, local_ssrc, local_video_payloadtype, 1412
					, 0, 0);
		}




		if(obj->tlvfile){
			unsigned char buf[16];

			tlv_file_write(obj->tlvfile, TLV_TYPE_MAGIC, UDP_FILE_MAGIC_SIZE, (void *)UDP_FILE_MAGIC);

			le_set_u32(portcount, buf);
			tlv_file_write(obj->tlvfile, TLV_TYPE_TYPECOUNT, 4, buf);

			udp_channel * ch = obj->channels;
			while(ch){
				le_set_u32(ch->index, buf+0);
				le_set_u32(ch->local_port,  buf+4);
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
			dbgi("port %d send: src %lld dst %lld packets", ch->local_port, ch->src.packet_count, ch->dst.packet_count);
			ch = ch->next;
		}
	}

	if(obj->tlvfile){
		tlv_file_close(obj->tlvfile);
		obj->tlvfile = NULL;
	}

	if(video_ch){
		if(video_ch->src.nalu_buf){
			free(video_ch->src.nalu_buf);
			video_ch->src.nalu_buf = NULL;
		}

		if(video_ch->dst.nalu_buf){
			free(video_ch->dst.nalu_buf);
			video_ch->dst.nalu_buf = NULL;
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

