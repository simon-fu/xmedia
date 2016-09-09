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
#include "xcutil.h"
#include "xrtp_h264.h"
#include "xtrans_codec.h"

#define dbgv(...) do{  printf("<rtp_replay>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{  printf("<rtp_replay>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{  printf("<rtp_replay>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)

typedef struct udp_channel{
	int port;
	int index;

	int sock;
	struct sockaddr_in  addrto;
	long long packet_count;

	int media_type;

	int nalu_buf_size;
	unsigned char * nalu_buf;
	union{
		xrtp_h264_repacker video_repacker;
		xrtp_transformer audio_transformer;
	};
	xrtp_transformer video_transformer;

	// uint32_t dst_ssrc;
	// uint32_t dst_payloadtype;
	// uint32_t src_packet_count;
	// int64_t src_first_ts;
	// xtimestamp64 tswrapper;
	// uint32_t dst_seq;

	// uint32_t src_samplerate;
	// uint32_t src_channels;
	// uint32_t dst_samplerate;
	// uint32_t dst_channels;

	// // audio
	// audio_transcoder_t * audio_transcoder;

	// // video
	// xrtp_to_nalu_t rtp2nalu;
	// xnalu_to_rtp_t nalu2rtp;

	// int got_sps;
	// int got_pps;
	// int got_keyframe;

}udp_channel;


#define MEDIA_UNKNOWN 0
#define MEDIA_AUDIO 1
#define MEDIA_VIDEO 2

static
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

static
const char * xnalu_type_string(int nalu_type){
	if(nalu_type == 5) return "(I)";
	if(nalu_type == 7) return "(SPS)";
	if(nalu_type == 8) return "(PPS)";
	return "";
}

static
void send_directly(udp_channel * ch, unsigned char * data, int data_len){
	int ret;
	ret = sendto(ch->sock, data, data_len, 0, (struct sockaddr*)&ch->addrto, sizeof(ch->addrto));
	ch->packet_count++;
}

// static
// uint32_t process_change_timestamp(udp_channel * ch, uint32_t timestamp, unsigned char * data, int data_len){
// 		if(ch->dst_samplerate > 0 && ch->src_samplerate > 0){
// 			// uint32_t timestamp = be_get_u32(data+4);
// 			int64_t t64 = timestamp;
// 			// dbgi("timestame1 = %u", timestamp);
// 			timestamp = (t64 * ch->dst_samplerate/ch->src_samplerate);	
// 			be_set_u32(timestamp, data+4);		
// 		}
// 		return timestamp;
// }

// static
// void process_repack_audio(udp_channel * ch, uint32_t timestamp, unsigned char * data, int data_len){
// 	int ret = 0;
// 		if(ch->dst_ssrc > 0){
// 			be_set_u32(ch->dst_ssrc, data+8);
// 		}

// 		if(ch->dst_payloadtype > 0){
// 			data[1] &= 0x80;
// 			data[1] |= (uint8_t)( (ch->dst_payloadtype & 0x7f) );
// 		}

// 		timestamp = process_change_timestamp(ch, timestamp, data, data_len);

// 		ret = sendto(ch->sock, data, data_len, 0, (struct sockaddr*)&ch->addrto, sizeof(ch->addrto));	
// 		ch->packet_count++;
// }

// static
// void dump_rtp(const char * prefix, unsigned char * data, int data_len){
// 	unsigned char first = data[0];
// 	uint32_t mask = (data[1] >> 7) & 0x1;
// 	uint32_t pt = (data[1] >> 0) & 0x7F;
// 	uint32_t seq = be_get_u16(data+2);
// 	uint32_t ts = be_get_u32(data+4);
// 	dbgi("%s first=%02X, pt=%u, seq=%u, ts=%u, %s", prefix, first, pt, seq, ts, mask?"Mask":"");

// }

// static
// void process_repack_video(udp_channel * ch, uint32_t timestamp, unsigned char * data, int data_len){
// 	int ret = 0;

// 		// dump_rtp("video rtp1", data, data_len);
// 		int mask = (data[1] >> 7) & 0x1;
// 		ret = xrtp_to_nalu_next(&ch->rtp2nalu, data, data_len);
// 		if(ret <= 0) return;
// 		int nalu_len = ret;
// 		// dbgi("video nalu timestamp %u, len=%d", timestamp, nalu_len);

// 		int nal_type = ch->nalu_buf[0] & 0x1F;
// 		if(nal_type == 5 || nal_type == 7  || nal_type == 8 ){
// 			dbgi("nalu: type=%d%s, len=%d", nal_type, xnalu_type_string(nal_type), nalu_len);	
// 		}
		
// 		if(nal_type == 9) return;

// 		// if(nal_type == 7) {// sps
// 		// 	if(ch->got_sps) return;
// 		// 	ch->got_sps = 1;
// 		// }else if(nal_type == 8){ // pps
// 		// 	if(ch->got_pps) return;
// 		// 	ch->got_pps = 1;
// 		// }else if(nal_type == 5){
// 		// 	ch->got_keyframe = 1;
// 		// 	dbgi("got keyframe");
// 		// }else if(nal_type == 9){
// 		// 	return;
// 		// }else{
// 		// 	if(!ch->got_keyframe){
// 		// 		return;
// 		// 	}
// 		// }

		
		
// 		ret = xnalu_to_rtp_first(&ch->nalu2rtp, timestamp, ch->nalu_buf, nalu_len, mask); 
// 		if(ret == 0){
// 			int len;
// 			while ((len = xnalu_to_rtp_next(&ch->nalu2rtp, data)) > 0) {
// 				// dump_rtp("video rtp2", data, len);
// 				ret = sendto(ch->sock, data, len, 0, (struct sockaddr*)&ch->addrto, sizeof(ch->addrto));	
// 				ch->packet_count++;
// 			}
// 		}
// }
// 
// static
// void process_rtp_repack(udp_channel * ch, uint32_t timestamp, unsigned char * data, int data_len){

// 	int ret = 0;
// 	if(ch->media_type == MEDIA_AUDIO){
// 		// timestamp = process_change_timestamp(ch, timestamp, data, data_len);
// 		process_repack_audio(ch, timestamp, data, data_len);
// 	}else if(ch->media_type == MEDIA_VIDEO){
// 		process_repack_video(ch, timestamp, data, data_len);
// 		// send_directly(ch, timestamp, data, data_len);
// 	}
// }

// static
// void process_rtp_transcode_half(udp_channel * ch, uint32_t timestamp, unsigned char * data, int data_len){
// 	int ret = 0;
// 	if(ch->media_type == MEDIA_AUDIO){

// 		uint32_t timestamp = be_get_u32(data+4);
// 		int64_t t64 = timestamp;
// 		// dbgi("timestame1 = %u", timestamp);
// 		timestamp = (t64 * ch->dst_samplerate/ch->src_samplerate);


// 		// dbgi("timestame2 = %u", timestamp);
// 		int ret = audio_transcoder_input(ch->audio_transcoder, (uint32_t)timestamp, data+12, data_len-12);
// 		if (ret < 0) {
// 			dbge("audio_transcoder_input error %d", ret);
// 			return ;
// 		}

// 		int trans_len;
		
// 		while((trans_len = audio_transcoder_next(ch->audio_transcoder, &timestamp, data + 12, 1600-12)) > 0){
// 			// t64 = timestamp;
// 			// timestamp = (t64 * ch->src_samplerate/ch->dst_samplerate);
// 			// dbgi("timestame3 = %u", timestamp);
// 			uint8_t mask = 1;
// 			uint8_t pt = ch->dst_payloadtype;
// 			uint32_t ssrc = ch->dst_ssrc;
// 			uint8_t *h = data;
// 			h[0] = 0x80;
// 			h[1] = (uint8_t)((pt & 0x7f) | ((mask & 0x01) << 7));
// 			be_set_u16(ch->dst_seq, h+2);
// 			be_set_u32(timestamp, h+4);
// 			be_set_u32(ssrc, h+8);

// 			ch->dst_seq = (ch->dst_seq + 1) & 0xffff;
// 			data_len = trans_len + 12;

// //			dbgd("on_rtmp_speex: rtp ts=%d, len=%d\n", timestamp, (trans_len+XRTP_HEADER_LEN));

// 			//process_repack_audio(ch, timestamp, data, data_len);
// 			ret = sendto(ch->sock, data, data_len, 0, (struct sockaddr*)&ch->addrto, sizeof(ch->addrto));	
// 			ch->packet_count++;
// 		}
// 		// dbgi("timestame done");

		
// 	}else if(ch->media_type == MEDIA_VIDEO){
// 		process_repack_video(ch, timestamp, data, data_len);
// 	}
// }

// static
// void process_rtp(udp_channel * ch, unsigned char * data, int data_len){
// 	unsigned char pkttype = data[0];
// 	if(pkttype > 0x9F){
// 		dbgi("drop non-rtp packet");
// 		return;
// 	}

// 	int64_t timestamp64 = xtimestamp64_unwrap(&ch->tswrapper, be_get_u32(data+4));
// 	if(ch->src_packet_count == 0){
// 		ch->src_first_ts = timestamp64;
// 		dbgi("src %d first timestamp %lld", ch->port, ch->src_first_ts);
// 	}
// 	ch->src_packet_count++;
	
// 	uint32_t timestamp = (uint32_t)(timestamp64 - ch->src_first_ts);
// 	be_set_u32(timestamp, data+4);

// 	process_rtp_repack(ch, timestamp, data, data_len);
// 	// process_rtp_transcode_half(ch, timestamp, data, data_len);
// }

static
void process_rtp(udp_channel * ch, unsigned char * data, int data_len){

	int ret = 0;
	if(ch->media_type == MEDIA_AUDIO){
		xrtp_transformer_process(&ch->audio_transformer, data);
		send_directly(ch, data, data_len);
	}else if(ch->media_type == MEDIA_VIDEO){
		// xrtp_transformer_process(&ch->video_transformer, data);
		// send_directly(ch, data, data_len);
		
		ret = xrtp_h264_repacker_input(&ch->video_repacker, data, data_len);
		if(ret > 0){
			int len;
			while ((len = xrtp_h264_repacker_next(&ch->video_repacker, data)) > 0) {
				// dump_rtp("video rtp2", data, len);
				send_directly(ch, data, len);
				// ret = sendto(ch->sock, data, len, 0, (struct sockaddr*)&ch->addrto, sizeof(ch->addrto));	
				// ch->packet_count++;
			}
		}
		
	}
}



int main(int argc, char** argv){
	// if(argc <= 1){
	// 	dbgi("usage: %s filename [ip]", argv[0]);
	// 	return -1;
	// }

	int src_audio_port = 10000;
	int src_video_port = 10002;


	uint32_t dst_ssrc = 0x1234;
	uint32_t dst_audio_payloadtype = 120;
	uint32_t dst_video_payloadtype = 96;

	// const char * dst_ip = "127.0.0.1";
	const char * dst_ip = "172.17.3.161"; // note3
	// const char * dst_ip = "172.17.1.247"; // huawei
	// const char * dst_ip = "172.17.3.7"; // raoshangrong kurento
	// const char * dst_ip = "192.168.124.206"; // vm kurento
	int dst_audio_port = 10000;
	int dst_video_port = 10002;
	
	
	#if 0
	// voice -> rtc
	int src_samplerate = 16000;
	int dst_samplerate = 48000;
	#else
	// rtc -> voice
	int src_samplerate = 48000;
	int dst_samplerate = 16000;

	#endif



	const char * filename = argv[1];
	int portcount = 0;
	udp_channel * channels = NULL;
	FILE * fpin = NULL;
	unsigned int buf_size = 1*1024*1024;
	unsigned char * buf = (unsigned char *)malloc(buf_size);
	int type;
	int length;

	
	int ret = 0;


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

		udp_channel * video_ch = NULL;
		udp_channel * audio_ch = NULL;
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

			udp_channel * ch = &channels[index];
			if(port == src_audio_port){
				ch->sock = create_udp_socket(0);
				init_socket_addrin(dst_ip, dst_audio_port, &ch->addrto);
				ch->media_type = MEDIA_AUDIO;

				xrtp_transformer_init(&ch->audio_transformer, src_samplerate, dst_samplerate, dst_audio_payloadtype, dst_ssrc );
				audio_ch = ch;

			}else if(port == src_video_port){
				ch->sock = create_udp_socket(0);
				init_socket_addrin(dst_ip, dst_video_port, &ch->addrto);
				ch->media_type = MEDIA_VIDEO;

				ch->nalu_buf_size = 1*1024*1024;
				ch->nalu_buf = (unsigned char *) malloc(ch->nalu_buf_size);
				xrtp_h264_repacker_init(&ch->video_repacker
						, ch->nalu_buf, ch->nalu_buf_size, 0
						, dst_ssrc, dst_video_payloadtype, 1412
						, 0, 0);
				xrtp_transformer_init(&ch->video_transformer, 0, 0, dst_video_payloadtype, dst_ssrc );
				video_ch = ch;

			}

		}

		if(!dst_ip){
			ret = 0;
			break;
		}

		if(audio_ch){
			udp_channel * ch = audio_ch;
			dbgi("audio: %d -> %s:%d, src_ar=%d, dst_ar=%d, dst_ssrc=%d dst_pt=%d"
				, ch->port, dst_ip, dst_audio_port
				, src_samplerate, dst_samplerate
				, dst_ssrc, dst_audio_payloadtype);
		}

		if(video_ch){
			udp_channel * ch = video_ch;
			dbgi("video: %d -> %s:%d, dst_ssrc=%d dst_pt=%d", ch->port, dst_ip, dst_video_port, dst_ssrc, dst_video_payloadtype);
		}

		dbgi("replay ...");
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
			udp_channel * ch = &channels[index];

			if( length > 8 && ch->sock > 0){
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

				process_rtp(ch, data, datalength);
				// dbgv("elapsed %lld: port %d sends No.%lld packet", get_timestamp_ms()-time_start, ch->port, ch->packet_count);
			}


		}
		if(ret < 0) break;

		dbgi("send successfully, use time %lld ms", get_timestamp_ms()-time_start);
		for(int i = 0; i < portcount; i++){
			udp_channel * ch = &channels[i];
			if(ch->sock > 0){
				dbgi("port %d sends %lld packets", ch->port, ch->packet_count);	
			}
		}

		ret = 0;
	}while(0);

	if(fpin){
		fclose(fpin);
		fpin = NULL;
	}

	for(int i = 0; i < portcount; i++){
		udp_channel * ch = &channels[i];
		if(ch->sock > 0){
			close(ch->sock);
			ch->sock = -1;
		}
	}

	if(channels){
		free(channels);
		channels = NULL;
	}

	
	return 0;
}

