

#ifndef xtlv_file_hpp
#define xtlv_file_hpp

#include <stdio.h>

#define TLV_TYPE_EOF      11
#define TLV_TYPE_CODEC    2000
#define TLV_TYPE_SDP      2001
#define TLV_TYPE_RTP      2002
#define TLV_TYPE_RTCP     2003

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
int tlv_file_read_raw(FILE * fp, void * buf, int bufsize, int * plength, int * ptype);

    
#endif /* xtlv_file_hpp */
