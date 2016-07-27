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
#include "xrtp_h264.h"

#define dbgv(...) do{  printf("<d2h264>[D] " __VA_ARGS__); printf("\n"); }while(0)
#define dbgi(...) do{  printf("<d2h264>[I] " __VA_ARGS__); printf("\n"); }while(0)
#define dbge(...) do{  printf("<d2h264>[E] " __VA_ARGS__); printf("\n"); }while(0)


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
	if(argc < 3){
		dbgi("usage: %s dumpfile h264file [port]", argv[0]);
		return -1;
	}

	const char * filename = argv[1];
	const char * outfilename = argv[2];
	const char * ip = NULL;
	int media_port = 0;
	int portcount = 0;
	FILE * fpin = NULL;
	FILE * fpout = NULL;
	unsigned int buf_size = 1*1024*1024;
	unsigned char * buf = (unsigned char *)malloc(buf_size);
	int type;
	int length;

	
	int ret = 0;


	if(argc > 3 ){
		media_port = atoi(argv[3]);
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


		int media_index = -1;
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
			dbgi("index %d, port %d", index, port);

			if(media_port > 0 && media_port == port){
				media_port = port;
				media_index = index;
			}
			
		}
		if(ret < 0) break;

		if(media_port == 0){
			ret = 0;
			break;
		}

		if(media_index < 0){
			dbge("can't find port %d", media_port);
			ret = -1; 
			break;
		}
		dbgi("find port index: %d -> %d", media_port, media_index);

		fpout = fopen(outfilename, "wb");
		if(!fpout){
			dbge("fail to open %s", outfilename);
			ret = -1;
			break;
		}
		dbgi("successfully open %s", outfilename);

		int64_t time_start = get_timestamp_ms();;
		xrtp_to_nalu_t converter_stor;
		xrtp_to_nalu_t * converter = &converter_stor;
		unsigned int nalu_buf_size = 1*1024*1024;
		unsigned char * nalu_buf = (unsigned char *)malloc(nalu_buf_size);
		int nalu_count = 0;
		int packet_count = 0;
		unsigned char annex[4] = {0, 0, 0, 1};

		ret = xrtp_to_nalu_init(converter, nalu_buf, nalu_buf_size, 0);
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


			if( length > 8 && media_index == index){
				int64_t pts = 0;
				unsigned int t32;

				t32 = le_get_u32(buf+0);
				pts |= t32;

				t32 = le_get_u32(buf+4);
				pts |= ((int64_t)t32) << 32;

				unsigned char * data = buf + 8;
				int datalength = length - 8;
				ret = xrtp_to_nalu_next(converter, data, datalength);
				if(ret > 0){
					int nal_type = nalu_buf[0] & 0x1F;
					dbgi("No.%d pkt: nalu: len=%d, type=%d%s, data=[%02X %02X %02X %02X]", packet_count, datalength, nal_type, nal_type==5?"(I)":""
						, nalu_buf[0], nalu_buf[1], nalu_buf[2], nalu_buf[3]);
					fwrite(annex, 1, sizeof(annex), fpout);
					fwrite(nalu_buf, 1, ret, fpout);
					nalu_count++;
				}
				ret = 0;
				packet_count++;
			}


		}
		if(ret < 0) break;

		dbgi("convert successfully, use time %lld ms, packet_count=%d nalu_count=%d", get_timestamp_ms()-time_start, packet_count, nalu_count);

		ret = 0;
	}while(0);

	if(fpin){
		fclose(fpin);
		fpin = NULL;
	}

	if(fpout){
		fclose(fpout);
		fpout = NULL;
	}
	
	return 0;
}

