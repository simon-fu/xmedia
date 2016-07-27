#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h> 
#include <netdb.h>

#include <chrono>
#include "util.h"

#define dbgv(...) do{  printf("<udp_replay>[D] " __VA_ARGS__); printf("\n"); }while(0)
#define dbgi(...) do{  printf("<udp_replay>[I] " __VA_ARGS__); printf("\n"); }while(0)
#define dbge(...) do{  printf("<udp_replay>[E] " __VA_ARGS__); printf("\n"); }while(0)

typedef struct udp_channel{
	int port;
	int index;

	struct sockaddr_in  addrto;
	long long packet_count;

}udp_channel;


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

#define UDP_FILE_MAGIC  		"UDPv1"
#define UDP_FILE_MAGIC_SIZE		6

static
int check_magic(FILE * fpin, unsigned char * buf, int buf_size, const char * magic){
	int ret = 0;
	int type;
	int length;
	do{
		ret = read_tlv(fpin, buf, buf_size, &length, &type);
		if(ret < 0){
			dbge("fail to read magic, ret=%d", ret);
			break;
		}
		if(type != TLV_TYPE_MAGIC){
			dbge("expect magic type %d but %d", TLV_TYPE_MAGIC, type);
			ret = -1;
			break;
		}
		if(strcmp((char *)buf, magic) != 0){
			dbge("expect magic [%s] but [%s]", magic, (char *)buf);
			ret = -1;
			break;
		}
		dbgi("magic %s", (char *)buf);
	}while(0);
	return ret;
}



int main(int argc, char** argv){
	if(argc <= 1){
		dbgi("usage: %s filename [ip]", argv[0]);
		return -1;
	}

	const char * filename = argv[1];
	const char * ip = NULL;
	int portcount = 0;
	udp_channel * channels = NULL;
	FILE * fpin = NULL;
	unsigned int buf_size = 1*1024*1024;
	unsigned char * buf = (unsigned char *)malloc(buf_size);
	int type;
	int length;
	int sock = -1;
	
	int ret = 0;

	if(argc > 2 ){
		ip = argv[2];
	}

	do{
		fpin = fopen(filename, "rb");
		if(!fpin){
			dbge("fail to open input file %s\n", filename);
			ret = -1;
			break;
		}
		dbgi("successfully reading open %s", filename);

		ret = check_magic(fpin, buf, buf_size, UDP_FILE_MAGIC);
		if(ret < 0){
			break;
		}

		ret = read_tlv(fpin, &portcount, sizeof(int), &length, &type);
		if(ret < 0){
			dbge("fail to type count tlv, ret=%d", ret);
			break;
		}
		if(type != TLV_TYPE_TYPECOUNT){
			dbge("expect typecount type %d but %d", TLV_TYPE_TYPECOUNT, type);
			ret = -1;
			break;
		}
		if(portcount <= 0){
			dbgi("error port count %d", portcount);
			ret = -1;
			break;
		}
		dbgi("port count %d", portcount);

		ZERO_ALLOC(channels, udp_channel *, (portcount*sizeof(udp_channel)));


		for(int i = 0; i < portcount; i++){
			unsigned char typeinfo[8];
			int typeinfo_length = 8;
			ret = read_tlv(fpin, typeinfo, 8, &length, &type);
			if(ret < 0){
				dbge("fail to type info tlv, ret=%d", ret);
				break;
			}
			if(type != TLV_TYPE_TYPEINFO){
				dbge("expect typeinfo type %d but %d", TLV_TYPE_TYPEINFO, type);
				ret = -1;
				break;
			}
			if(length != typeinfo_length){
				dbge("expect typeinfo length %d but %d", typeinfo_length, length);
				ret = -1;
				break;
			}
			int index = le_get_u32(typeinfo+0);
			int port = le_get_u32(typeinfo+4);
			if(index >= portcount){
				dbge("expect index less than %d but %d", portcount, index);
				ret = -1;
				break;
			}
			channels[index].port = port;
			dbgi("index %d, port %d", index, port);
		}

		if(!ip){
			ret = 0;
			break;
		}

		ret = create_udp_socket(0);
		if(ret < 0){
			dbge("fail to create udp socket, ret=%d", ret);
			break;
		}
		sock = ret;
		dbgi("successfully create UDP socket %d", sock); // TODO: print local port

		for(int i = 0; i < portcount; i++){
			udp_channel * ch = &channels[i];
			init_socket_addrin(ip, ch->port, &ch->addrto);
		}

		dbgi("replay to %s ...", ip);
		int64_t ts_start = -1;
		int64_t time_start = -1;
		while(1){
			ret = read_tlv(fpin, buf, buf_size, &length, &type);
			if(ret < 0){
				dbge("reach file end, ret=%d", ret);
				ret = 0;
				break;
			}

			if(length < 8){
				dbge("udp data length too short, %d", length);
				ret = -1;
				break;
			}
			if(type >= portcount){
				dbge("expect udp type less than %d, but %d", portcount, type);
			}
			int index = type;

			if( length > 8){
				int64_t pts = 0;
				unsigned int t32;

				t32 = le_get_u32(buf+0);
				pts |= t32;

				t32 = le_get_u32(buf+4);
				pts |= ((int64_t)t32) << 32;

				unsigned char * data = buf + 8;
				int datalength = length - 8;

				int64_t now = get_timestamp_ms();
				if(ts_start < 0){
					ts_start = pts;
					time_start = now;
				}

				int64_t time_elapsed = now - time_start;
				if(pts > ts_start ){
					int64_t ts_elapsed = pts - ts_start;
					if(ts_elapsed > time_elapsed){
						usleep((ts_elapsed-time_elapsed)*1000);
					}
				}

				udp_channel * ch = &channels[index];
				ret = sendto(sock, data, datalength, 0, (struct sockaddr*)&ch->addrto, sizeof(ch->addrto));
				ch->packet_count++;
				// dbgv("elapsed %lld: port %d sends No.%lld packet", get_timestamp_ms()-time_start, ch->port, ch->packet_count);
			}


		}
		if(ret < 0) break;

		dbgi("send successfully, use time %lld ms", get_timestamp_ms()-time_start);
		for(int i = 0; i < portcount; i++){
			udp_channel * ch = &channels[i];
			dbgi("port %d sends %lld packets", ch->port, ch->packet_count);
		}

		ret = 0;
	}while(0);

	if(fpin){
		fclose(fpin);
		fpin = NULL;
	}
	
	if(sock >= 0){
		close(sock);
		sock = -1;
	}

	if(channels){
		free(channels);
		channels = NULL;
	}

	
	return 0;
}

