


#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
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


#ifdef __cplusplus
}
#endif

#endif


