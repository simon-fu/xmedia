#ifndef __XC_UTIL_H__
#define __XC_UTIL_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// #include "plw.h"
// #include "xsys_layer.h"

#if 1
#ifdef WIN32
#define LOCK_T			plw_mutex_t
#define LOCK_NEW(x)		plw_mutex_create(&(x))
#define LOCK_DEL(x)		plw_mutex_delete(x)
#define LOCK(x)         plw_mutex_lock(x)
#define UNLOCK(x) 		plw_mutex_unlock(x)
#define COND_T					plw_event_t
#define COND_NEW(x)				plw_event_create(&(x))
#define COND_DEL(x)				plw_event_delete(x)
#define COND_WAIT(x, xlock)		do{ UNLOCK(xlock); plw_event_wait(x); LOCK(xlock); }while(0)
#define COND_SIGNAL(x)			plw_event_set(x)
#else
#include <pthread.h>
#define LOCK_T        pthread_mutex_t
#define LOCK_NEW(x)		pthread_mutex_init(&(x),NULL)
#define LOCK_DEL(x)		pthread_mutex_destroy(&x)
#define LOCK(x)         pthread_mutex_lock(&(x))
#define UNLOCK(x) 		pthread_mutex_unlock(&(x))
#define COND_T					pthread_cond_t
#define COND_NEW(x)				pthread_cond_init(&(x), NULL)
#define COND_DEL(x)				pthread_cond_destroy(&(x))
#define COND_WAIT(x, xlock)		pthread_cond_wait(&(x), &(xlock))
#define COND_SIGNAL(x)			pthread_cond_signal(&(x))
#endif // #ifdef WIN32
#else
#define LOCK_T					int
#define LOCK_NEW(x)
#define LOCK_DEL(x)
#define LOCK(x)
#define UNLOCK(x)
#define COND_T					plw_event_t
#define COND_NEW(x)				plw_event_create(&(x))
#define COND_DEL(x)				plw_event_delete(x)
#define COND_WAIT(x, xlock)		do{ UNLOCK(xlock); plw_event_wait(x); LOCK(xlock); }while(0)
#define COND_SIGNAL(x)			plw_event_set(x)
#endif

#ifdef __cplusplus

class llock
{
protected:
	LOCK_T &lock;
public:
	llock(LOCK_T &_lock)
		:lock(_lock)
	{
		LOCK(lock);
	}
	~llock()
	{
		UNLOCK(lock);
	}
};


#endif

#ifdef __cplusplus
extern "C" {
#endif



/*
* big-endian read/write
*/


#define     be_get_u16(xptr)   ((unsigned short) ( ((xptr)[0] << 8) | ((xptr)[1] << 0) ))     /* by simon */
#define     be_set_u16(xval, xptr)  \
do{\
    (xptr)[0] = (unsigned char) (((xval) >> 8) & 0x000000FF);\
    (xptr)[1] = (unsigned char) (((xval) >> 0) & 0x000000FF);\
}while(0)



#define     be_get_u24(xptr)   ((unsigned int) ( ((xptr)[0] << 16) | ((xptr)[1] << 8) | ((xptr)[2] << 0)))      /* by simon */
#define     be_set_u24(xval, xptr)  \
do{\
    (xptr)[0] = (unsigned char) (((xval) >> 16) & 0x000000FF);\
    (xptr)[1] = (unsigned char) (((xval) >> 8) & 0x000000FF);\
    (xptr)[2] = (unsigned char) (((xval) >> 0) & 0x000000FF);\
}while(0)




#define     be_get_u32(xptr)   ((unsigned int) ((((unsigned int)(xptr)[0]) << 24) | ((xptr)[1] << 16) | ((xptr)[2] << 8) | ((xptr)[3] << 0)))        /* by simon */
#define     be_set_u32(xval, xptr)  \
do{\
    (xptr)[0] = (unsigned char) (((xval) >> 24) & 0x000000FF);\
    (xptr)[1] = (unsigned char) (((xval) >> 16) & 0x000000FF);\
    (xptr)[2] = (unsigned char) (((xval) >> 8) & 0x000000FF);\
    (xptr)[3] = (unsigned char) (((xval) >> 0) & 0x000000FF);\
}while(0)




/*
* little-endian read/write
*/

#define     le_get_u16(xptr)   (unsigned short) ( ((xptr)[1] << 8) | ((xptr)[0] << 0) )
#define     le_set_u16(xval, xptr)  \
do{\
    (xptr)[1] = (unsigned char) (((xval) >> 8) & 0x000000FF);\
    (xptr)[0] = (unsigned char) (((xval) >> 0) & 0x000000FF);\
}while(0)


#define     le_get_u24(xptr)   (unsigned short) ( ((xptr)[2] << 16) | ((xptr)[1] << 8) | ((xptr)[0] << 0) )
#define     le_set_u24(xval, xptr)  \
do{\
    (xptr)[2] = (unsigned char) (((xval) >> 16) & 0x000000FF);\
    (xptr)[1] = (unsigned char) (((xval) >> 8) & 0x000000FF);\
    (xptr)[0] = (unsigned char) (((xval) >> 0) & 0x000000FF);\
}while(0)



#define     le_get_u32(xptr)   (unsigned int) ((xptr)[3] << 24) | ((xptr)[2] << 16) | ((xptr)[1] << 8) | ((xptr)[0] << 0)
#define     le_set_u32(xval, xptr)  \
do{\
    (xptr)[3] = (unsigned char) (((xval) >> 24) & 0x000000FF);\
    (xptr)[2] = (unsigned char) (((xval) >> 16) & 0x000000FF);\
    (xptr)[1] = (unsigned char) (((xval) >> 8) & 0x000000FF);\
    (xptr)[0] = (unsigned char) (((xval) >> 0) & 0x000000FF);\
}while(0)






#define endian_u16_swap(x)     ( ((x) << 8) & 0xff00) | ( ((x) >> 8) & 0x00ff) )
#define endian_u32_swap(x)     ( ( ((x) << 24) & 0xff000000) | (((x) << 16) & 0x00ff0000) |  (((x) >> 16) & 0x0000ff00) | (((x) >> 24) & 0x000000ff))







#ifdef PLW_ENDIAN_LITTLE

#define endian_u16_big_to_host(x)       endian_u16_swap(x)
#define endian_u32_big_to_host(x)       endian_u32_swap(x)

#define endian_u16_little_to_host(x)    (x)
#define endian_u32_little_to_host(x)    (x)

#else

#define endian_u16_big_to_host(x)       (x)
#define endian_u32_big_to_host(x)       (x)

#define endian_u16_little_to_host(x)    endian_u16_swap(x)
#define endian_u32_little_to_host(x)    endian_u32_swap(x)

#endif



#define ZERO_ALLOC(o, type, sz) do{o=(type)malloc(sz); memset(o, 0, sz);}while(0)

// static
// void * zero_alloc(size_t sz){
//     void * p = malloc(sz);
//     memset(p, 0, sz);
//     return p;
// }


#define TLV_TYPE_MAGIC      1000
#define TLV_TYPE_TYPECOUNT  1001
#define TLV_TYPE_TYPEINFO   1002

int64_t get_timestamp_ms();
int create_udp_socket(int port);
int read_tlv(FILE * fp, void * buf, int bufsize, int * plength, int * ptype);




void xdebug_save_to_default_file(const char * file_name, const void * data, uint32_t length);
void xdebug_write_to_file(void ** ctx, const char * file_name, const void * data, uint32_t length);

//#define TRIM_SIZE sizeof(size_t)
#define TRIM_SIZE 8

inline int trim_mem_size(int sz){
	int mod = sz%TRIM_SIZE;
	return sz + (mod > 0 ? TRIM_SIZE-mod : 0);
}
#define TRIM_STRUCT_SIZE(sz) trim_mem_size(sz)

long long xustime(void);
long long xmstime(void);


#ifdef __cplusplus
} // extern "C"
#endif

#endif
