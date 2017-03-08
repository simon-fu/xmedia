#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "rav_record.h"
#include "util.h"

#define dbgd(...) do{  printf("[DEBUG] " __VA_ARGS__);}while(0)
#define dbgi(...) do{  printf("[INFO ] " __VA_ARGS__);}while(0)
#define dbgw(...) do{  printf("[WARN ] " __VA_ARGS__);}while(0)
#define dbge(...) do{  printf("[ERROR] " __VA_ARGS__);}while(0)







typedef struct mfile_st * mfile_t;
typedef int (*mfile_loop_func_t)(mfile_t obj, int frame_no, void * context, int64_t pts, unsigned char * buf, int length);


typedef struct mfile_info{
	
	unsigned int header_length;
	char codec[4];
	unsigned int video_width;
	unsigned int video_height;
	unsigned int rate;
	unsigned int timescale;
	unsigned int frame_count;

}mfile_info;

struct mfile_st{
	FILE * fp;
	unsigned char header[32];
	mfile_info file_info;
	int frame_no;
};



void mfile_close(mfile_t obj){
	if(obj->fp){
		fclose(obj->fp);
		obj->fp = 0;
	}
}

mfile_t mfile_open(const char * filename, mfile_info * file_info){
	int ret = 0;
	mfile_t obj = 0;


	do{
		obj = (mfile_t) zero_alloc(sizeof(struct mfile_st));
		obj->fp = fopen(filename, "rb");
		if(obj->fp == NULL) {
			dbge("could not open file %s\n", filename);
			ret = -1;
			break;
		}

		ret = 0;

	}while(0);

	if(ret){
		mfile_close(obj);
		obj = 0;
	}
	return obj;
}



int mfile_loop(mfile_t obj, void * context, mfile_loop_func_t cbfunc){
	int frame_length;
	int64_t pts;
	unsigned char t4[4];
	int bytes;
	unsigned int t32;
	unsigned int data_buf_size = 1*1024*1024;
	unsigned char * data_buf = (unsigned char *) malloc(data_buf_size);
	int frame_no = 0;

	while(1){
		pts = 0;

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			break;
		}
		frame_length = le_get_u32(t4);

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			break;
		}
		t32 = le_get_u32(t4);
		pts |= t32;

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			break;
		}
		t32 = le_get_u32(t4);
		pts |= ((int64_t)t32) << 32;

		bytes = fread(data_buf, sizeof(char), frame_length, obj->fp);
		if(bytes != frame_length){
			break;
		}

		cbfunc(obj, frame_no, context, pts, data_buf, frame_length);
		frame_no++;
	}
	return frame_no;
}


int mfile_read(mfile_t obj, unsigned char * data_buf, int64_t * p_pts, int * p_length, int * p_frame_no){
	int frame_length;
	int64_t pts;
	unsigned char t4[4];
	int bytes;
	unsigned int t32;
	int frame_no = 0;
	int ret = 0;

	do{
		pts = 0;

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			ret = -1;
			break;
		}
		frame_length = le_get_u32(t4);

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			ret = -1;
			break;
		}
		t32 = le_get_u32(t4);
		pts |= t32;

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			ret = -1;
			break;
		}
		t32 = le_get_u32(t4);
		pts |= ((int64_t)t32) << 32;

		bytes = fread(data_buf, sizeof(char), frame_length, obj->fp);
		if(bytes != frame_length){
			ret = -1;
			break;
		}

		//cbfunc(obj, frame_no, context, pts, data_buf, frame_length);
		*p_pts = pts;
		*p_length = frame_length;
		*p_frame_no = obj->frame_no;

		obj->frame_no++;

		ret = 0;
	}while(0);

	return ret;
}



typedef struct ivffile_st * ivffile_t;
typedef int (*ivffile_loop_func_t)(ivffile_t obj, int frame_no, void * context, int64_t pts, unsigned char * buf, int length);


typedef struct ivffile_info{
	
	unsigned int header_length;
	char codec[4];
	unsigned int video_width;
	unsigned int video_height;
	unsigned int rate;
	unsigned int timescale;
	unsigned int frame_count;
	
}ivffile_info;

struct ivffile_st{
	FILE * fp;
	unsigned char header[32];
	ivffile_info file_info;
	int frame_no;
};





static 
int parse_fileheader(const unsigned char * header, ivffile_info * file_info){
	if(memcmp(header, "DKIF", 4) != 0){
		return -1;
	}
	file_info->header_length = le_get_u16(header+6);
	memcpy(file_info->codec, header+8, 4);
	file_info->video_width = le_get_u16(header+12);
	file_info->video_height = le_get_u16(header+14);
	file_info->rate = le_get_u32(header+16);
	file_info->timescale = le_get_u32(header+20);
	file_info->frame_count = le_get_u32(header+24);
	return 0;
}

void ivfile_dump_info(const ivffile_info * file_info){
	dbgi("ivfile info:\n");
	dbgi("  header_length=%d\n", file_info->header_length);
	dbgi("  codec=%c%c%c%c\n", file_info->codec[0], file_info->codec[1], file_info->codec[2], file_info->codec[3]);
	dbgi("  video_width=%d\n", file_info->video_width);
	dbgi("  video_height=%d\n", file_info->video_height);
	dbgi("  rate=%d\n", file_info->rate);
	dbgi("  timescale=%d\n", file_info->timescale);
	dbgi("  frame_count=%d\n", file_info->frame_count);
}

void ivfile_dump(ivffile_t obj){
	ivfile_dump_info(&obj->file_info);
}

void ivffile_close(ivffile_t obj){
	if(obj->fp){
		fclose(obj->fp);
		obj->fp = 0;
	}
}

ivffile_t ivffile_open(const char * filename, ivffile_info * file_info){
	int ret = 0;
	ivffile_t obj = 0;


	do{
		obj = (ivffile_t) zero_alloc(sizeof(struct ivffile_st));
		obj->fp = fopen(filename, "rb");
		if(obj->fp == NULL) {
			dbge("could not open file %s\n", filename);
			ret = -1;
			break;
		}

		int bytes = fread(obj->header, sizeof(char), 32, obj->fp);
		if(bytes != 32){
			dbge("read ivf file header fail !!!\n");
			ret = -1;
			break;
		}

		ret = parse_fileheader(obj->header, &obj->file_info);
		if(ret){
			dbge("parse ive file header fail!!!\n");
			ret = -1;
			break;
		}

		if(obj->file_info.header_length != 32){
			dbge("unknown ive file header length %d!!!\n", obj->file_info.header_length);
			ret = -1;
			break;
		}

		if(file_info){
			memcpy(file_info, &obj->file_info, sizeof(struct ivffile_info));
		}

		ret = 0;

	}while(0);

	if(ret){
		ivffile_close(obj);
		obj = 0;
	}
	return obj;
}



int ivffile_loop(ivffile_t obj, void * context, ivffile_loop_func_t cbfunc){
	int frame_length;
	int64_t pts;
	unsigned char t4[4];
	int bytes;
	unsigned int t32;
	unsigned int data_buf_size = 1*1024*1024;
	unsigned char * data_buf = (unsigned char *) malloc(data_buf_size);
	int frame_no = 0;

	while(1){
		pts = 0;

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			break;
		}
		frame_length = le_get_u32(t4);

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			break;
		}
		t32 = le_get_u32(t4);
		pts |= t32;

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			break;
		}
		t32 = le_get_u32(t4);
		pts |= ((int64_t)t32) << 32;

		bytes = fread(data_buf, sizeof(char), frame_length, obj->fp);
		if(bytes != frame_length){
			break;
		}

		cbfunc(obj, frame_no, context, pts, data_buf, frame_length);
		frame_no++;
	}
	return frame_no;
}

int ivffile_read(ivffile_t obj, unsigned char * data_buf, int64_t * p_pts, int * p_length, int * p_frame_no){
	int frame_length;
	int64_t pts;
	unsigned char t4[4];
	int bytes;
	unsigned int t32;
	int frame_no = 0;
	int ret = 0;

	do{
		pts = 0;

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			ret = -1;
			break;
		}
		frame_length = le_get_u32(t4);

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			ret = -1;
			break;
		}
		t32 = le_get_u32(t4);
		pts |= t32;

		bytes = fread(t4, sizeof(char), 4, obj->fp);
		if(bytes != 4){
			ret = -1;
			break;
		}
		t32 = le_get_u32(t4);
		pts |= ((int64_t)t32) << 32;

		bytes = fread(data_buf, sizeof(char), frame_length, obj->fp);
		if(bytes != frame_length){
			ret = -1;
			break;
		}

		//cbfunc(obj, frame_no, context, pts, data_buf, frame_length);
		*p_pts = pts;
		*p_length = frame_length;
		*p_frame_no = obj->frame_no;

		obj->frame_no++;

		ret = 0;
	}while(0);

	return ret;

}






static 
int ivf_cb(ivffile_t obj, int frame_no, void * context, int64_t pts, unsigned char * buf, int length){
	dbgi("ivf frame: No.%d, pts=%lld, length=%d\n", frame_no, pts, length);
	ravrecord_t rr = (ravrecord_t ) context;
	int ret = rr_process_video(rr, pts, buf, length);
	return 0;
}

static 
int audio_mfile_cb(mfile_t obj, int frame_no, void * context, int64_t pts, unsigned char * buf, int length){
	// dbgi("audio frame: No.%d, pts=%lld, length=%d\n", frame_no, pts, length);
	// ravrecord_t rr = (ravrecord_t ) context;
	// int ret = rr_process_pcm(rr, pts, buf, length);

	// FILE * fp = (FILE*) context;
	// fwrite(buf, 1, length, fp); 

	return 0;
}


int main0(int argc, char *argv[]){
	ravrecord_t rr = 0;
	ivffile_t ivf = 0;
	ivffile_info ivfinfo;
	mfile_t mf = 0;
	const char * input = "02_vp8.ivf";
	const char * audio_input = "02_mixer_0x7f5aec003990.audio";
	// const char * output = "output.mkv";
	const char * output = "output.webm";

	unsigned int video_buf_size = 1*1024*1024;
	unsigned char * video_buf = (unsigned char *) malloc(video_buf_size);

	unsigned int audio_buf_size = 1*1024*1024;
	unsigned char * audio_buf = (unsigned char *)malloc(video_buf_size);


	dbgi("vidoe input : %s\n", input);
	dbgi("audio input : %s\n", audio_input);
	dbgi("output: %s\n", output);

	 FILE * _audioFile4Debug = fopen("output.pcm", "wb");

	do{

		ivf = ivffile_open(input, &ivfinfo);
		if(!ivf){
			break;
		}
		ivfile_dump(ivf);

		mf = mfile_open(audio_input, NULL);
		if(!mf){
			break;
		}

// ravrecord_t rr_open(const char * output_file_name
// 	, mediacodec_id_t video_codec_id, int width, int height, int fps
// 	, mediacodec_id_t audio_codec_id, int audio_samplerate, int audio_channels);

		// rr = rr_open(output, ivfinfo.video_width, ivfinfo.video_height, 15, RR_FMT_RTP_VP8);
		rr = rr_open(output, MCODEC_ID_VP8, 300, 200, 15
			, MCODEC_ID_OPUS, 48000, 2, 48000, 2, NULL, 0);
		if(!rr){
			break;
		}

		// int frame_count = ivffile_loop(ivf, rr, ivf_cb);
		// dbgi("frame_count=%d\n", frame_count);

		int video_frame_count = 0;
		int64_t video_pts = 0;
		int video_length = 0;
		int video_frame_no = 0;

		int audio_frame_count = 0;
		int64_t audio_pts = 0;
		int audio_length = 0;
		int audio_frame_no = 0;


		unsigned char * video_keyframe = (unsigned char *)malloc(256*1024);
		int video_keyframe_length = 0;

		long loopcount = 0;
		int ret;
		while(1){
			
			if(video_pts >= 0 && (video_pts <= audio_pts || audio_pts < 0)){
				int video_ret = ivffile_read(ivf, video_buf, &video_pts, &video_length, &video_frame_no);
				if(video_ret == 0){
					dbgi("video frame: No.%d, pts=%lld, length=%d\n", video_frame_no, video_pts, video_length);
					int keyFrame = !(video_buf[0]&0x1);
					if(keyFrame){
						dbgi("buffer video key frame, length=%d\n", video_length);
						memcpy(video_keyframe, video_buf, video_length);
						video_keyframe_length = video_length;
					}

					//video_ret = rr_process_video(rr, video_pts, video_buf, video_length);
					if(video_frame_count >= 100){
						if(video_keyframe_length > 0){
							dbgi("set video key frame, length=%d\n", video_keyframe_length);
							video_ret = rr_process_video(rr, video_pts, video_keyframe, video_keyframe_length);
							video_keyframe_length = 0;
						}
						video_ret = rr_process_video(rr, video_pts, video_buf, video_length);
					}
					
					video_frame_count++;
				}else{
					dbgi("video reach end\n");
					video_pts = -1;
				}
			}
			
			if(audio_pts >= 0 && (audio_pts <= video_pts || video_pts < 0)){
				int audio_ret = mfile_read(mf, audio_buf, &audio_pts, &audio_length, &audio_frame_no);
				if(audio_ret == 0){
					dbgi("audio frame: No.%d, pts=%lld, length=%d\n", audio_frame_no, audio_pts, audio_length);

					if(video_frame_count >= 100){
						audio_ret = rr_process_pcm(rr, audio_pts, audio_buf, audio_length);
					}
					
					audio_frame_count++;
				}else{
					dbgi("audio reach end\n");
					audio_pts = -1;
				}
			}

			if(video_pts < 0 && audio_pts < 0) break;
			loopcount++;

		}
		dbgi("video_frame_count=%d\n", video_frame_count);
		dbgi("audio_frame_count=%d\n", audio_frame_count);

		// // int audio_frame_count = mfile_loop(mf, _audioFile4Debug, audio_mfile_cb);
		// int audio_frame_count = mfile_loop(mf, rr, audio_mfile_cb);
		// dbgi("audio_frame_count=%d\n", audio_frame_count);
	// 
	
	}while(0);

	if(_audioFile4Debug){
		fclose(_audioFile4Debug);
		_audioFile4Debug = NULL;
	}

	if(ivf){
		ivffile_close(ivf);
		ivf = NULL;
	}

	if(mf){
		mfile_close(mf);
		mf = NULL;
	}

	if(rr){
		rr_close(rr);
		rr = NULL;
	}

	return 0;
}

static
int read_tlv(FILE * fp, unsigned char * buf, int bufsize, int * plength, int * ptype){
	int ret = 0;
	int type;
	int length;
	int bytes;
	unsigned char t4[4];

	do
	{
		bytes = fread(t4, sizeof(char), 2, fp);
		if(bytes != 2){
			ret = -1;
			break;
		}
		type = le_get_u16(t4);
		// dbgi("tlv: type=%d\n", type);

		bytes = fread(t4, sizeof(char), 4, fp);
		if(bytes != 4){
			break;
		}
		length = le_get_u32(t4);

		if(length > bufsize){
			dbge("too big tlv length %d, bufsize=%d\n", length, bufsize);
			ret = -1;
			break;
		}

		bytes = fread(buf, sizeof(char), length, fp);
		if(bytes != length){
			ret = -1;
			break;
		}

		*ptype = type;
		*plength = length;

		ret = 0;
	} while (0);
	return ret;
}


#define TLV_TYPE_TYPELIST    1000
#define TLV_TYPE_VIDEO_INFO  1001
#define TLV_TYPE_AUDIO_INFO  1002
#define TLV_TYPE_PCM_INFO    1003

int main1(int argc, char *argv[]){

	
	const char * input_file = "test.tlv";
	// const char * output_file = "output.mkv";
	const char * output_file = "output.webm";
	const char * output_pcm_file = "output.pcm";

	unsigned int video_buf_size = 1*1024*1024;
	unsigned char * video_buf = (unsigned char *) malloc(video_buf_size);

	unsigned int buf_size = 1*1024*1024;
	unsigned char * buf = (unsigned char *)malloc(video_buf_size);

	ravrecord_t rr = 0;

	dbgi("input_file : %s\n", input_file);
	dbgi("audio output_file : %s\n", output_file);
	dbgi("output_pcm_file: %s\n", output_pcm_file);

	FILE * fpin = NULL;
	FILE * fppcm = NULL;
	int ret = 0;

	do{
		fpin = fopen(input_file, "rb");
		if(!fpin){
			dbge("fail to open input file %s\n", input_file);
			ret = -1;
			break;
		}

		fppcm = fopen(output_pcm_file, "wb");
		if(!fppcm){
			dbge("fail to open output pcm file %s\n", output_pcm_file);
			ret = -1;
			break;
		}

		int video_codec_id = MCODEC_ID_NONE, video_width = 0, video_height = 0;
		int audio_codec_id = MCODEC_ID_NONE, audio_samplerate = 0, audio_channels = 0;
		int playout_codec_id = MCODEC_ID_NONE, playout_samplerate = 0, playout_channels = 0;

		int type = 0;
		int length = 0;
		while(1){
			ret = read_tlv(fpin, buf, buf_size, &length, &type);
			if(ret < 0){
				break;
			}
			dbgi("header: type=%d, length=%d\n", type, length);
			if(type == TLV_TYPE_VIDEO_INFO){
				video_codec_id = le_get_u32(buf+0);
				video_width = le_get_u32(buf+4);
				video_height = le_get_u32(buf+8);
			}else if(type == TLV_TYPE_AUDIO_INFO){
				audio_codec_id = le_get_u32(buf+0);
				audio_samplerate = le_get_u32(buf+4);
				audio_channels = le_get_u32(buf+8);
			}else if(type == TLV_TYPE_PCM_INFO){
				playout_codec_id = le_get_u32(buf+0);
				playout_samplerate = le_get_u32(buf+4);
				playout_channels = le_get_u32(buf+8);
			}else{
				break;
			}
			type = 0;
			length = 0;
		}
		if(ret < 0) break;

		dbgi("video info: codec_id=%d, resolution=%dx%d\n", video_codec_id, video_width, video_height);
		dbgi("audio info: codec_id=%d, ar=%d, ch=%d\n", audio_codec_id, audio_samplerate, audio_channels);
		dbgi("pcm   info: codec_id=%d, ar=%d, ch=%d\n", playout_codec_id, playout_samplerate, playout_channels);

// ravrecord_t rr_open(const char * output_file_name
// 	, mediacodec_id_t video_codec_id, int width, int height, int fps
// 	, mediacodec_id_t audio_codec_id, int audio_samplerate, int audio_channels);

// ravrecord_t rr_open(const char * output_file_name
// 	, mediacodec_id_t video_codec_id, int width, int height, int fps
// 	, mediacodec_id_t audio_codec_id, int audio_samplerate, int audio_channels,int playout_samplerate,int playout_channels
//         ,unsigned char *sps_pps_buffer, int  sps_pps_length);

		rr = rr_open(output_file, (mediacodec_id_t)video_codec_id, video_width, video_height, 15
			, (mediacodec_id_t)playout_codec_id, playout_samplerate, playout_channels
			, playout_samplerate, playout_channels
			, NULL, 0);
		if(!rr){
			break;
		}

		int video_frames = 0;
		int pcm_frames = 0;
		do
		{
			if(length >= 8){
				int64_t pts = 0;
				unsigned int t32;

				t32 = le_get_u32(buf+0);
				pts |= t32;

				t32 = le_get_u32(buf+4);
				pts |= ((int64_t)t32) << 32;

				unsigned char * data = buf + 8;
				int datalength = length - 8;

				if(type == video_codec_id){
					// dbgi("video pts %lld\n", (long long)pts);
					video_frames++;
					ret = rr_process_video(rr, pts, data, datalength);
				}else if(type == audio_codec_id){
					// dbgi("audio pts %lld\n", (long long)pts);
					ret = rr_process_audio(rr, pts, data, datalength);
				}else if(type == playout_codec_id){
					// dbgi("pcm pts %lld\n", (long long)pts);
					pcm_frames++;
					// ret = rr_process_pcm(rr, pts, data, datalength);
					ret = fwrite(data, 1, datalength, fppcm);
				}else{
					dbgi("unknown type %d\n", type);
				}
			}

			ret = read_tlv(fpin, buf, buf_size, &length, &type);
			if(ret < 0){
				ret = 0;
				break;
			}
		} while (1);

		dbgi("video_frames =%d\n", video_frames);
		dbgi("pcm_frames =%d\n", pcm_frames);

	
	}while(0);

	if(fpin){
		fclose(fpin);
		fpin = NULL;
	}

	if(fppcm){
		fclose(fppcm);
		fppcm = NULL;
	}

	if(rr){
		rr_close(rr);
		rr = NULL;
	}

	return 0;

}

int main(int argc, char *argv[]){
	return main1(argc, argv);
}
