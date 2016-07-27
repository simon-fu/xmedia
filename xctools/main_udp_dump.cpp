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

#define dbgv(...) do{  printf("<udp_dump>[D] " __VA_ARGS__); printf("\n"); }while(0)
#define dbgi(...) do{  printf("<udp_dump>[I] " __VA_ARGS__); printf("\n"); }while(0)
#define dbge(...) do{  printf("<udp_dump>[E] " __VA_ARGS__); printf("\n"); }while(0)

typedef struct tlv_file_st * tlv_file_t;




typedef struct udp_dumper_st *udp_dumper_t;

typedef struct udp_channel{
	struct udp_channel * next;
	udp_dumper_t owner;
	int sock;
	int port;
	struct event * ev_udp;
	long long packet_count;
	char buff[2*1024];
	int index;
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
void on_udp_event(evutil_socket_t fd, short what, void *arg){
	// dbgv("on_udp_event");
	if(!(what&EV_READ)) return ;
	udp_channel * ch = (udp_channel *) arg;

    struct sockaddr_in tempadd;
    ev_socklen_t len = sizeof(tempadd);
    int bytes_to_recv = sizeof(ch->buff);
    
    int sock_ret = recvfrom(ch->sock, ch->buff, bytes_to_recv, 0, (struct sockaddr*)&tempadd, &len);
    if(sock_ret <= 0){
    	dbge("recvfrom fail with %d", sock_ret);
    	return;
    }
 //    const char *from_ip = inet_ntoa(tempadd.sin_addr);
	// int from_port = ntohs(tempadd.sin_port);
 //    dbgv("port %d recv %d bytes from %s:%d", ch->port, sock_ret, from_ip, from_port);

	if(ch->owner->tlvfile){
		int64_t ts = get_timestamp_ms();
		tlv_file_write2(ch->owner->tlvfile, ch->index, sizeof(ts), &ts, sock_ret, ch->buff);
		ch->packet_count++;
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
	if(argc <= 2){
		dbgi("usage: %s filename port1 [port2 port3 ...]", argv[0]);
		return -1;
	}

	udp_dumper_t obj = NULL;
	// const char * filename = "./dump.udp";
	// int ports[2] = {10000, 10002};
	// int portcount = 2;

	const char * filename = argv[1];
	int portcount = argc-2;
	int * ports_buf = (int *) malloc(portcount*sizeof(int));
	int * ports = ports_buf;
	
	for(int i = 0; i < portcount; i++){
		ports[i] = atoi(argv[2+i]);
	}
	int ret = 0;

	do{
		ZERO_ALLOC(obj, udp_dumper_t, sizeof(struct udp_dumper_st));
		obj->tlvfile = tlv_file_open(filename);
		if(!obj->tlvfile){
			dbge("fail to writing open %s", filename);
			ret = -1;
			break;
		}
		dbgi("successfully writing open %s", filename);

		obj->ev_base = event_base_new();
		obj->ev_signal = evsignal_new(obj->ev_base, SIGINT, ev_signal_cb, (void *)obj);
		event_add(obj->ev_signal, NULL);

		ret = 0;
		for(int i = 0; i < portcount; i++){
			dbgi("add udp port %d", ports[i]);
			udp_channel * ch = create_and_add_udp_channel(obj, ports[i]);
			ch->index = i;
			if(!ch){
				ret = -1;
				break;
			}
		}
		if(ret) break;

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


		// loop 
		dbgi("start dumping...");
		event_base_dispatch(obj->ev_base);
		ret = 0;
	}while(0);

	if(ret == 0){
		udp_channel * ch = obj->channels;
		while(ch){
			dbgi("port %d writes %lld packets", ch->port, ch->packet_count);
			ch = ch->next;
		}
	}

	if(obj->tlvfile){
		tlv_file_close(obj->tlvfile);
		obj->tlvfile = NULL;
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

	if(ports_buf){
		free(ports_buf);
		ports_buf = NULL;
	}
	
	return 0;
}

