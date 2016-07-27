
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include "util.h"


int64_t get_timestamp_ms(){ 
    unsigned long milliseconds_since_epoch = 
    std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();
    return milliseconds_since_epoch;
};

int create_udp_socket(int port){

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sock;
    if ( (sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
        return -1;
    }

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        return -1;
    }
    return sock;

}


int read_tlv(FILE * fp, void * buf, int bufsize, int * plength, int * ptype){
	int ret = 0;
	int type;
	int length;
	int bytes;
	unsigned char t4[4];

	do
	{
		bytes = fread(t4, sizeof(char), 4, fp);
		if(bytes != 4){
			ret = -11;
			break;
		}
		type = le_get_u32(t4);
		// dbgi("tlv: type=%d\n", type);

		bytes = fread(t4, sizeof(char), 4, fp);
		if(bytes != 4){
			ret = -12;
			break;
		}
		length = le_get_u32(t4);

		if(length > bufsize){
			// printf("too big tlv length %d, bufsize=%d\n", length, bufsize);
			ret = -1000;
			break;
		}

		bytes = fread(buf, sizeof(char), length, fp);
		if(bytes != length){
			ret = -13;
			break;
		}

		*ptype = type;
		*plength = length;

		ret = 0;
	} while (0);
	return ret;
}
