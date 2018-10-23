

#ifndef xtlv_file_hpp
#define xtlv_file_hpp

#include <stdio.h>

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

#define     be_get_i64(xptr)   ((int64_t) \
((((int64_t)(xptr)[0]) << 56) \
| ((int64_t)(xptr)[1] << 48) \
| ((int64_t)(xptr)[2] << 40) \
| ((int64_t)(xptr)[3] << 32) \
| ((int64_t)(xptr)[4] << 24) \
| ((int64_t)(xptr)[5] << 16) \
| ((int64_t)(xptr)[6] << 8) \
| ((int64_t)(xptr)[7] << 0)))

#define     be_set_i64(xval, xptr)  \
do{\
(xptr)[0] = (unsigned char) (((xval) >> 56) & 0x000000FF);\
(xptr)[1] = (unsigned char) (((xval) >> 48) & 0x000000FF);\
(xptr)[2] = (unsigned char) (((xval) >> 40) & 0x000000FF);\
(xptr)[3] = (unsigned char) (((xval) >> 32) & 0x000000FF);\
(xptr)[4] = (unsigned char) (((xval) >> 24) & 0x000000FF);\
(xptr)[5] = (unsigned char) (((xval) >> 16) & 0x000000FF);\
(xptr)[6] = (unsigned char) (((xval) >> 8) & 0x000000FF);\
(xptr)[7] = (unsigned char) (((xval) >> 0) & 0x000000FF);\
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





#define TLV_TYPE_EOF      11
#define TLV_TYPE_CODEC    2000
#define TLV_TYPE_SDP      2001
#define TLV_TYPE_RTP      2002
#define TLV_TYPE_RTCP     2003
#define TLV_TYPE_TIME_STR           3000
#define TLV_TYPE_TIME_JSON          3010
#define TLV_TYPE_TIME_PORT_RECVUDP  3020
#define TLV_TYPE_TIME_PORT_SENDUDP  3021
#define TLV_TYPE_TIME_PORT_ADD      3031
#define TLV_TYPE_TIME_PORT_REMOVE   3032

// =======>  TLV_TYPE_CODEC
//      u32 --> codec id
//      u32 --> payload type
//      u32 --> samplerate
//      u32 --> channels
//      u32 --> codec name length
//      char[] --> codec name

// =======>  TLV_TYPE_SDP
//      char[] --> sdp string

// =======>  TLV_TYPE_RTP/TLV_TYPE_RTCP
//      u32 --> high 32bit of milli-seconds
//      u32 --> low  32bit of milli-seconds
//      u8[] --> RTP/RTCP data


typedef struct tlv_file_st * tlv_file_t;
tlv_file_t tlv_file_open(const char * filename);
void tlv_file_close(tlv_file_t obj);
int tlv_file_write(tlv_file_t obj, int type, int length, void * value);
int tlv_file_write2(tlv_file_t obj, int type, int length1, void * value1, int length2, void * value2);
int tlv_file_writen(tlv_file_t obj, int type, int fix_length, void * fix_value
                    , int lengths[], void * values[], int num, int * plength=NULL);
//int tlv_file_write_ts(tlv_file_t obj, int type, int64_t ts, int length, void * value);
int tlv_file_read_raw(FILE * fp, void * buf, int bufsize, int * plength, int * ptype);



#endif /* xtlv_file_hpp */
