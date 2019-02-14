

#include <arpa/inet.h>
// #include <endian.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>


extern "C"{    
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

//#include "dbg.hpp"
//#include "util.h"
#include "rav_record.h"

#define dbgi(...) 
#define dbgw(...)
#define dbge(...)

static
void * zero_alloc(size_t sz){
    void * p = malloc(sz);
    memset(p, 0, sz);
    return p;
}


/* WebRTC stuff (VP8/VP9) */
#if defined(__ppc__) || defined(__ppc64__)
	# define swap2(d)  \
	((d&0x000000ff)<<8) |  \
	((d&0x0000ff00)>>8)
#else
	# define swap2(d) d
#endif


struct ravrecord {
	//dont use std::string, it will segv on gcc, maybe related to zero_alloc?
	char filename[1024];
	//FILE * fp;
	AVFormatContext *fctx;
    int write_header;
	AVStream *vStream;
	AVStream *aStream;
	AVCodecContext * audio_encoder_context;
	AVFrame * pcm_frame;
	//unsigned char audio_encoded_buf[48000];
	//unsigned char audio_pcm_buf[48000*2*2]; // max_samples * channels * (sizeof s16) 
	int pcm_frame_in_bytes;
	int pcm_length;
	int64_t audio_next_pts;
	int64_t audio_pts;
	int64_t pts_base;
	enum AVCodecID video_codecid;
	bool check_first_intra_frame;
};



static
AVStream * new_video_stream(AVFormatContext * fctx, enum AVCodecID codecid, int max_width, int max_height, int fps,unsigned char *sps_pps_buffer, int  sps_pps_length){
	int ret = 0;
	AVStream *stream = NULL;
	do{
		stream = avformat_new_stream(fctx, 0);
		if(stream == NULL) {
			dbge("Error new video stream\n");
			ret = -1;
			break;
		}

        stream->codecpar->codec_id = codecid;
        stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        stream->time_base = (AVRational){1, fps};
        stream->codecpar->width = max_width;
        stream->codecpar->height = max_height;
        stream->codecpar->format = AV_PIX_FMT_YUV420P;
		// following line resolves webm file decode error in chrome 56.0 (windows)
		stream->codecpar->field_order = AV_FIELD_PROGRESSIVE;

        if(codecid == AV_CODEC_ID_H264)
        {
            stream->codecpar->extradata = new uint8_t[sps_pps_length];
            memcpy(stream->codecpar->extradata, sps_pps_buffer,sps_pps_length);
            stream->codecpar->extradata_size = sps_pps_length;
        }
	}while(0);

	return stream;

}

// todo: audio transcoding part is broken
static 
AVStream * new_audio_stream(ravrecord_t rr, AVFormatContext * fctx, enum AVCodecID codecid, int sample_rate, int channels, int bit_rate){
	int ret = 0;
	AVStream *stream = NULL;
	do{
		const AVCodec *codec = avcodec_find_encoder(codecid);
		stream = avformat_new_stream(fctx, codec);
		if(stream == NULL) {
			dbge("Error new audio stream\n");
			ret = -1;
			break;
		}

        //avcodec_get_context_defaults3(stream->codec, NULL);
        stream->codecpar->codec_id = codecid;
        stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        stream->codecpar->sample_rate = sample_rate;
        stream->codecpar->channels = channels;
        //stream->codecpar->bit_rate = bit_rate <= 0 ? 24000 : bit_rate;
        stream->codecpar->format = AV_SAMPLE_FMT_S16;
        stream->time_base = (AVRational){ 1, sample_rate };
        stream->codecpar->channel_layout = channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
        
    	dbgi("sample_rate=%d, channels=%d, channel_layout=%d, bit_rate=%d\n"
    		, stream->codecpar->sample_rate
    		, stream->codecpar->channels
    		, (int)stream->codecpar->channel_layout
    		, stream->codecpar->bit_rate);

//		if (fctx->flags & AVFMT_GLOBALHEADER)
//			stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

//		// init encoder 
        if(AV_CODEC_ID_MP3 == codecid){
	        dbge( "avcodec_open2\n");

			rr->audio_encoder_context = avcodec_alloc_context3(codec);
            if (!rr->audio_encoder_context) {
                dbge("rav_record: avcodec_alloc_context3 failed\n");
                exit(-1);
            }
            rr->audio_encoder_context->sample_fmt = AV_SAMPLE_FMT_S16;
            rr->audio_encoder_context->bit_rate = bit_rate ? 24000 : bit_rate;
            rr->audio_encoder_context->sample_rate = sample_rate;
            rr->audio_encoder_context->channels = channels;
            rr->audio_encoder_context->channel_layout = channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
            //rr->audio_encoder_context->time_base = stream->time_base;

		    ret = avcodec_open2(rr->audio_encoder_context, codec, NULL);
		    if (ret < 0) {
		        dbge( "audio avcodec_open2 fail with %d\n", ret);
		        break;
		    }

		    AVFrame *frame = av_frame_alloc();
		    if (!frame) {
		        dbge("Error allocating audio frame\n");
		        ret = -1;
		        break;
		    }
		    rr->pcm_frame = frame;

		    frame->format = rr->audio_encoder_context->sample_fmt;
            frame->channels = rr->audio_encoder_context->channels;
		    frame->channel_layout = rr->audio_encoder_context->channel_layout;
		    frame->sample_rate = rr->audio_encoder_context->sample_rate;
		    frame->nb_samples = stream->codec->frame_size;

		    rr->pcm_frame_in_bytes = stream->codec->frame_size * stream->codec->channels * 2;
		    if(rr->pcm_frame_in_bytes == 0){
		    	rr->pcm_frame_in_bytes = 2* stream->codec->sample_rate/10;
		    }

		    //dbgi("stream audio frame_size=%d, pcm_frame_in_bytes=%d\n", stream->codec->frame_size, rr->pcm_frame_in_bytes);
		}

	    ret = 0;

	}while(0);

	if(ret){
		// TODO:
	}

	return stream;

}

ravrecord_t rr_open(const char * output_file_name
	, mediacodec_id_t video_codec_id, int width, int height, int fps
	, mediacodec_id_t audio_codec_id, int audio_samplerate, int audio_channels,int playout_samplerate,int playout_channels
        ,unsigned char *sps_pps_buffer, int  sps_pps_length){
	int ret = 0;
	ravrecord_t rr = 0;

//	int vp8 = 1;

#if LIBAVCODEC_VERSION_MAJOR < 55
	if(!vp8) {
		dbge("Your FFmpeg version does not support VP9\n");
		return NULL;
	}
#endif
        
	do{
		rr = (ravrecord_t) zero_alloc(sizeof(struct ravrecord));
		strncpy(rr->filename, output_file_name, sizeof(rr->filename) - 1);
		rr->fctx = avformat_alloc_context();
		if(rr->fctx == NULL) {
			dbge("error allocating context\n");
			ret = -1;
			break;
		}
		rr->check_first_intra_frame = true;
		rr->fctx->oformat = av_guess_format(NULL, output_file_name, NULL);
		if(rr->fctx->oformat == NULL) {
			dbge("Error guessing format\n");
			ret = -1;
			break;
		}
		snprintf(rr->fctx->filename, sizeof(rr->fctx->filename), "%s", output_file_name);
// 		if (rr->fctx->flags & AVFMT_GLOBALHEADER)
// 			rr->vStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        if(video_codec_id == MCODEC_ID_H264)
        {
          rr->video_codecid = AV_CODEC_ID_H264;
          rr->vStream = new_video_stream(rr->fctx, AV_CODEC_ID_H264, width, height, fps,sps_pps_buffer,sps_pps_length);
        }else if(video_codec_id == MCODEC_ID_VP8)
        {
           rr->video_codecid = AV_CODEC_ID_VP8;
           rr->vStream = new_video_stream(rr->fctx, AV_CODEC_ID_VP8, width, height, fps,NULL,0);
        }

        if(audio_codec_id == MCODEC_ID_OPUS)
        {
            rr->aStream = new_audio_stream(rr, rr->fctx, AV_CODEC_ID_OPUS, audio_samplerate, audio_channels, 24000);
        }else if(audio_codec_id == MCODEC_ID_AAC)
        {
            rr->aStream = new_audio_stream(rr, rr->fctx, AV_CODEC_ID_AAC, audio_samplerate, audio_channels, 24000);
        }

		if(int err = avio_open(&rr->fctx->pb, rr->fctx->filename, AVIO_FLAG_WRITE) < 0) {

			dbge("Error opening file for output, %d\n", err);
			ret = -1;
			break;
		}

        AVDictionary* opts = nullptr;
// this will let matroska_enc split cluster and add cue point to the output file,
// but, such file wont play in Chrome 56 (Windows)
//
//        if (strcmp(rr->fctx->oformat->name, "webm") == 0) {
//            // matroska only write cue point when dash is set to 1
//            av_dict_set(&opts, "dash", "1", 0);
//        }
		if(avformat_write_header(rr->fctx, &opts) < 0) {
			dbge("Error writing header\n");
			ret = -1;
			break;
		}
        if (rr->fctx->pb->seekable) {
            dbgi("seekable");
        } else {
            dbgi("not seekable");
        }
        if (opts) {
            AVDictionaryEntry* t = nullptr;
            while (t = av_dict_get(opts, "", t, 0)) {
                dbge("invalid opt `%s` found", t->key);
            }
        }
        rr->write_header = 1;

        ret = 0;

	}while(0);

	if(ret){
		rr_close(rr);
		rr = 0;
	}
	return rr;
}


void rr_close(ravrecord_t rr){
    if(!rr) return;
    if(rr->fctx != NULL && rr->write_header){
        av_write_trailer(rr->fctx);
    }

	if(rr->audio_encoder_context) {
        avcodec_free_context(&rr->audio_encoder_context);
        rr->audio_encoder_context = nullptr;
    }

    if (rr->pcm_frame) {
        av_frame_free(&rr->pcm_frame);
        rr->pcm_frame = nullptr;
    }

	if(rr->fctx != NULL) {
		avio_close(rr->fctx->pb);
        rr->fctx->pb = nullptr;
        avformat_free_context(rr->fctx);
        rr->fctx = nullptr;
	}
}

int rr_process_video(ravrecord_t rr, int64_t pts, unsigned char * buf, int length, bool is_key_frame){

	if(!rr->vStream){
		return -1;
	}
    
	if(rr->pts_base == 0){
		rr->pts_base = pts;
	}

    pts = pts - rr->pts_base;
    if(pts < 0){
        pts = 0;
    }
    
	if (rr->check_first_intra_frame) {
		if (!is_key_frame) {
			dbgw("discard inter frame without preceeding intra frame");
			return -1;
		} else {
			rr->check_first_intra_frame = false;
		}
	}

	AVPacket packet;
	av_init_packet(&packet);
	packet.stream_index = rr->vStream->index;
	packet.data = buf;
	packet.size = length;
    
    AVRational& time_base = rr->vStream->time_base;
    pts = pts * time_base.den/time_base.num/1000;
    
	if (is_key_frame){
		packet.flags |= AV_PKT_FLAG_KEY;
	}

	packet.dts = pts;
	packet.pts = pts;
	if(int err = av_interleaved_write_frame(rr->fctx, &packet) < 0) {
		dbge("write one video frame to file %s failed\n", rr->filename);
	}
	return 0;
}

int rr_process_audio(ravrecord_t rr, int64_t pts, unsigned char * buf, int length){

	if(!rr->aStream){
		return -1;
	}

	if(rr->pts_base == 0){
		rr->pts_base = pts;
	}

	if(pts < rr->pts_base){
		dbgw("drop audio frame (pts=%lld)\n", (long long) pts);
		return 0;
	}
	pts = pts - rr->pts_base;
    //printf("write audio: pts=%lld\n", pts);
    
	AVPacket packet;
	av_init_packet(&packet);
	packet.stream_index = rr->aStream->index;
	packet.data = buf;
	packet.size = length;

    //AVRational time_base = rr->aStream->time_base;
    //pts = (double)(pts*1000)/(double)(av_q2d(time_base)*AV_TIME_BASE);
    
    AVRational& time_base = rr->aStream->time_base;
    pts = pts * time_base.den/time_base.num/1000;
    
	packet.dts = pts;
	packet.pts = pts;
	if(av_interleaved_write_frame(rr->fctx, &packet) < 0) {
		dbge("write one audio frame to file %s failed\n", rr->filename);
	}
	return 0;
}
