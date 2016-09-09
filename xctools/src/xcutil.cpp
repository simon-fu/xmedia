
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <sys/time.h>
#include "xcutil.h"


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


static int
write_to_file(FILE * fp, const void * data, unsigned length)
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

static FILE * g_fp = 0;
void xdebug_save_to_default_file(const char * file_name, const void * data, uint32_t length){
	if(!g_fp){
		g_fp = fopen(file_name, "wb");
	}
	write_to_file(g_fp, data, length);
}

void xdebug_write_to_file(void ** ctx, const char * file_name, const void * data, uint32_t length){
	FILE * fp = (FILE *) (*ctx);
	if(!fp){
		fp = fopen(file_name, "wb");
		*ctx = fp;
	}
	write_to_file(fp, data, length);
}


long long xustime(void) {
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long)tv.tv_sec)*1000000;
    ust += tv.tv_usec;
    return ust;
}

long long xmstime(void) {
    struct timeval tv;
    long long mst;

    gettimeofday(&tv, NULL);
    mst = ((long long)tv.tv_sec)*1000;
    mst += tv.tv_usec/1000;
    return mst;
}



