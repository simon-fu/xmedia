#include "stdio.h"
#include "stdlib.h"
#include <sys/time.h>

#include "xutil.h"

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
