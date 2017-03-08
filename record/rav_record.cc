

#include <arpa/inet.h>
// #include <endian.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

extern "C"{    
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "util.h"
#include "rav_record.h"


#define dbgd(...) do{  printf("[D] " __VA_ARGS__);}while(0)
#define dbgi(...) do{  printf("[I] " __VA_ARGS__);}while(0)
#define dbgw(...) do{  printf("[W] " __VA_ARGS__);}while(0)


#define dbge(...) do{  printf("[E] " __VA_ARGS__);}while(0)


/* WebRTC stuff (VP8/VP9) */
#if defined(__ppc__) || defined(__ppc64__)
	# define swap2(d)  \
	((d&0x000000ff)<<8) |  \
	((d&0x0000ff00)>>8)
#else
	# define swap2(d) d
#endif

#define LIBAVCODEC_VER_AT_LEAST(major, minor) \
	(LIBAVCODEC_VERSION_MAJOR > major || \
	 (LIBAVCODEC_VERSION_MAJOR == major && \
	  LIBAVCODEC_VERSION_MINOR >= minor))



struct ravrecord {
	FILE * fp;
	AVFormatContext *fctx;
    int write_header;
	AVStream *vStream;
	AVStream *aStream;
	AVCodecContext * audio_encoder_context;
	AVFrame * pcm_frame;
	unsigned char audio_encoded_buf[48000];
	unsigned char audio_pcm_buf[48000*2*2]; // max_samples * channels * (sizeof s16) 
	int pcm_frame_in_bytes;
	int pcm_length;
	int64_t audio_next_pts;
	int64_t audio_pts;
	int64_t pts_base;
        enum AVCodecID video_codecid;
};



static
AVStream * new_video_stream(AVFormatContext * fctx, enum AVCodecID codecid, int max_width, int max_height, int fps,unsigned char *sps_pps_buffer, int  sps_pps_length){
	int ret = 0;
	AVStream *stream = NULL;
	do{
//		const AVCodec *codec = avcodec_find_encoder(codecid);
//		if(!codec){
//			codec = avcodec_find_decoder(codecid);
//		}
//		if(!codec){
//			const char * codec_name = avcodec_get_name(codecid)?avcodec_get_name(codecid):"";
//			dbge("Error finding video codecid %d (%s) \n", codecid, codec_name);
//			ret = -1;
//			break;
//		}
//		// const AVCodec *codec = NULL;

		//~ rr->vStream = av_new_stream(fctx, 0);
		stream = avformat_new_stream(fctx, 0);
		if(stream == NULL) {
			dbge("Error new video stream\n");
			ret = -1;
			break;
		}

//		avcodec_get_context_defaults3(stream->codec, codec);
        
        AVCodec codecStor;
        AVCodec * codec = &codecStor;
        memset(codec, 0, sizeof(AVCodec));
        codec->id = codecid;
        codec->type = AVMEDIA_TYPE_VIDEO;
        avcodec_get_context_defaults3(stream->codec, NULL);
        
		stream->codec->codec_id = codecid;
		stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
		stream->codec->time_base = (AVRational){1, fps};
		stream->codec->width = max_width;
		stream->codec->height = max_height;
		stream->codec->pix_fmt = PIX_FMT_YUV420P;
                if(codecid == AV_CODEC_ID_H264)
                {
                  stream->codec->extradata = new uint8_t[sps_pps_length];
                  memcpy(stream->codec->extradata, sps_pps_buffer,sps_pps_length);
                  stream->codec->extradata_size = sps_pps_length;
                }
		if (fctx->flags & AVFMT_GLOBALHEADER)
			stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}while(0);

	return stream;

}

static 
AVStream * new_audio_stream(ravrecord_t rr, AVFormatContext * fctx, enum AVCodecID codecid, int sample_rate, int channels, int bit_rate){
	int ret = 0;
	AVStream *stream = NULL;
	do{
		const AVCodec *codec = avcodec_find_encoder(codecid);
//        if(!codec){
//            codec = avcodec_find_decoder(codecid);
//        }
//		if(!codec){
//            const char * codec_name = avcodec_get_name(codecid)?avcodec_get_name(codecid):"";
//			dbge("Error finding audio codecid %d(%s)\n", codecid, codec_name);
//			ret = -1;
//			break;
//		}
//		// const AVCodec *codec = NULL;
        if(AV_CODEC_ID_MP3 == codecid){
		   stream = avformat_new_stream(fctx, codec);
		}
		else
		{
		 stream = avformat_new_stream(fctx, 0);
		}
		if(stream == NULL) {
			dbge("Error new audio stream\n");
			ret = -1;
			break;
		}

//		avcodec_get_context_defaults3(stream->codec, codec);
        
//        AVCodec codecStor;
//        AVCodec * codec = &codecStor;
//        memset(codec, 0, sizeof(AVCodec));
//        codec->id = codecid;
//        codec->type = AVMEDIA_TYPE_AUDIO;
        avcodec_get_context_defaults3(stream->codec, NULL);
        
        
		stream->codec->codec_id = codecid;
        stream->codec->codec_type = AVMEDIA_TYPE_AUDIO;

		stream->codec->sample_rate = sample_rate;
    	stream->codec->channels = channels;
    	// stream->codec->bit_rate = bit_rate <= 0 ? 24000 : bit_rate;
    	stream->codec->sample_fmt = AV_SAMPLE_FMT_S16;
    	// stream->codec->sample_fmt  = codec->sample_fmts ? (codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;

    	stream->codec->time_base.den = sample_rate;
    	stream->codec->time_base.num = 1;
    	stream->codec->channel_layout = channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;

    	// stream->time_base = stream->codec->time_base;

    	dbgi("sample_rate=%d, channels=%d, channel_layout=%d, bit_rate=%d\n"
    		, stream->codec->sample_rate
    		, stream->codec->channels
    		, (int)stream->codec->channel_layout
    		, stream->codec->bit_rate);

		if (fctx->flags & AVFMT_GLOBALHEADER)
			stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;


//		// init encoder 
        if(AV_CODEC_ID_MP3 == codecid){
	        dbge( "avcodec_open2\n");
			rr->audio_encoder_context = stream->codec;
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

		    frame->channels = stream->codec->channels;
		    frame->format = stream->codec->sample_fmt;
		    frame->channel_layout = stream->codec->channel_layout;
		    frame->sample_rate = stream->codec->sample_rate;
		    frame->nb_samples = stream->codec->frame_size;

		    rr->pcm_frame_in_bytes = stream->codec->frame_size * stream->codec->channels * 2;
		    if(rr->pcm_frame_in_bytes == 0){
		    	rr->pcm_frame_in_bytes = 2* stream->codec->sample_rate/10;
		    }

		    //dbgi("stream audio frame_size=%d, pcm_frame_in_bytes=%d\n", stream->codec->frame_size, rr->pcm_frame_in_bytes);
		}
//	    // if (frame->nb_samples > 0) {
//	    // 	dbgi("audio init nb_samples = %d\n", frame->nb_samples);
//	    //     ret = av_frame_get_buffer(frame, 0);
//	    //     if (ret < 0) {
//	    //         dbge("Error allocating an audio buffer\n");
//	    //         ret = -1;
//	    //         break;
//	    //     }
//	    // }

	    ret = 0;

	}while(0);

	if(ret){
		// TODO:
	}

	return stream;

}


// ravrecord_t rr_open(const char * output_file_name, int max_width, int max_height, int fps, RR_FMT_T fmt){
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
		av_register_all();
		rr = (ravrecord_t) zero_alloc(sizeof(struct ravrecord));
		
		rr->fctx = avformat_alloc_context();
		if(rr->fctx == NULL) {
			dbge("error allocating context\n");
			ret = -1;
			break;
		}


		// dump all codecs
		// AVCodec * c = NULL;
		// while((c=av_codec_next (c)) != NULL){
		// 	dbgi(" codec=%d(%s)(%d)\n", c->id, c->name, av_codec_is_decoder(c));
		// }

		//rr->fctx->oformat = av_guess_format("webm", NULL, NULL);
		rr->fctx->oformat = av_guess_format(NULL, output_file_name, NULL);
		if(rr->fctx->oformat == NULL) {
			dbge("Error guessing format\n");
			ret = -1;
			break;
		}
		snprintf(rr->fctx->filename, sizeof(rr->fctx->filename), "%s", output_file_name);




// 		//~ rr->vStream = av_new_stream(fctx, 0);
// 		rr->vStream = avformat_new_stream(rr->fctx, 0);
// 		if(rr->vStream == NULL) {
// 			dbge("Error adding stream\n");
// 			ret = -1;
// 			break;
// 		}

// 		//~ avcodec_get_context_defaults2(vStream->codec, CODEC_TYPE_VIDEO);
// // #if LIBAVCODEC_VER_AT_LEAST(53, 21)
// // 		avcodec_get_context_defaults3(rr->vStream->codec, AVMEDIA_TYPE_VIDEO);
// // #else
// // 		avcodec_get_context_defaults2(rr->vStream->codec, AVMEDIA_TYPE_VIDEO);
// // #endif
// 		avcodec_get_context_defaults3(rr->vStream->codec, avcodec_find_encoder(AV_CODEC_ID_VP8));

// #if LIBAVCODEC_VER_AT_LEAST(54, 25)
// 	#if LIBAVCODEC_VERSION_MAJOR >= 55
// 		rr->vStream->codec->codec_id = vp8 ? AV_CODEC_ID_VP8 : AV_CODEC_ID_VP9;
// 	#else
// 		rr->vStream->codec->codec_id = AV_CODEC_ID_VP8;
// 	#endif
// #else
// 		rr->vStream->codec->codec_id = CODEC_ID_VP8;
// #endif
// 		//~ vStream->codec->codec_type = CODEC_TYPE_VIDEO;
// 		rr->vStream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
// 		rr->vStream->codec->time_base = (AVRational){1, fps};
// 		rr->vStream->codec->width = max_width;
// 		rr->vStream->codec->height = max_height;
// 		rr->vStream->codec->pix_fmt = PIX_FMT_YUV420P;
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
                
		if(rr->vStream == NULL) {
			dbge("Error adding video stream\n");
			ret = -1;
			break;
		}

		//rr->aStream = new_audio_stream(rr, rr->fctx, AV_CODEC_ID_OPUS, audio_samplerate, audio_channels, 24000);
        if(video_codec_id == MCODEC_ID_H264)
        {
          rr->aStream = new_audio_stream(rr, rr->fctx, AV_CODEC_ID_MP3, playout_samplerate, playout_channels, 24000);
        }else if(video_codec_id == MCODEC_ID_VP8)
        {
          rr->aStream = new_audio_stream(rr, rr->fctx, AV_CODEC_ID_OPUS, audio_samplerate, audio_channels, 24000);
        }
		if(rr->aStream == NULL) {
			dbge("Error adding audio stream\n");
			ret = -1;
			break;
		}

		//~ rr->fctx->timestamp = 0;
		//~ if(url_fopen(&fctx->pb, fctx->filename, URL_WRONLY) < 0) {
		if(avio_open(&rr->fctx->pb, rr->fctx->filename, AVIO_FLAG_WRITE) < 0) {
			dbge("Error opening file for output\n");
			ret = -1;
			break;
		}

		//~ memset(&parameters, 0, sizeof(AVFormatParameters));
		//~ av_set_parameters(fctx, &parameters);
		//~ fctx->preload = (int)(0.5 * AV_TIME_BASE);
		//~ fctx->max_delay = (int)(0.7 * AV_TIME_BASE);
		//~ if(av_write_header(fctx) < 0) {
		if(avformat_write_header(rr->fctx, NULL) < 0) {
			dbge("Error writing header\n");
			ret = -1;
			break;
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
		

	if(rr->aStream != NULL && rr->aStream->codec != NULL)
		avcodec_close(rr->aStream->codec);

	if(rr->vStream != NULL && rr->vStream->codec != NULL)
		avcodec_close(rr->vStream->codec);

	// if(rr->fctx != NULL && rr->fctx->streams[0] != NULL) {
	// 	av_free(rr->fctx->streams[0]->codec);
	// 	av_free(rr->fctx->streams[0]);
	// }


	if(rr->fctx != NULL) {
		for(unsigned int ui = 0; ui < rr->fctx->nb_streams; ui++){
			if(rr->fctx->streams[ui] != NULL){
				av_free(rr->fctx->streams[ui]->codec);
				av_free(rr->fctx->streams[ui]);
				rr->fctx->streams[ui] = NULL;
			}
		}

		//~ url_fclose(fctx->pb);
		avio_close(rr->fctx->pb);
        av_free(rr->fctx); // avformat_free_context(rr->fctx);
	}
}

int rr_process_video(ravrecord_t rr, int64_t pts, unsigned char * buf, int length){

	if(!rr->vStream){
		return -1;
	}
    

	if(rr->pts_base == 0){
		rr->pts_base = pts;
	}
    int keyFrame = 0/*!(buf[0]&0x1)*/;
    if(rr->video_codecid == AV_CODEC_ID_VP8)
    {
      keyFrame = !(buf[0]&0x1);
    }else if (rr->video_codecid == AV_CODEC_ID_H264)
    {
      int naltype = 0;
      naltype = buf[4] & 0x0f;
      if ((naltype == 0x07) || (naltype==0x05) || (naltype == 0x08))
	{
	 keyFrame = 1;
	}
       
     }

	if(pts < rr->pts_base){
        if(!keyFrame){
            dbgw("drop video frame (pts=%lld)\n", (long long) pts);
            return 0;
        }else{
            dbgw("force key frame pts to base: %lld -> %lld\n", (long long) pts, rr->pts_base);
            pts = rr->pts_base;
        }
	}
	pts = pts - rr->pts_base;


//	memset(buf + length, 0, FF_INPUT_BUFFER_PADDING_SIZE);

	AVPacket packet;
	av_init_packet(&packet);
	packet.stream_index = rr->vStream->index;
	packet.data = buf;
	packet.size = length;
	
	if(keyFrame){
		//~ packet.flags |= PKT_FLAG_KEY;
		packet.flags |= AV_PKT_FLAG_KEY;
		dbgi("is video key frame packet\n");
	}

	/* First we save to the file... */
	//~ packet.dts = AV_NOPTS_VALUE;
	//~ packet.pts = AV_NOPTS_VALUE;

    AVRational time_base = rr->vStream->time_base;
    pts = (double)(pts*1000)/(double)(av_q2d(time_base)*AV_TIME_BASE);
	packet.dts = pts;
	packet.pts = pts;
//	dbgi("write video packet: size=%d, pts=%lld\n", packet.size, (long long)packet.pts);
	if(av_interleaved_write_frame(rr->fctx, &packet) < 0) {
		//dbge("Error writing video frame to file...\n");
	}
	return 0;
}

int rr_process_audio(ravrecord_t rr, int64_t pts, unsigned char * buf, int length){

	if(!rr->aStream){
		return -1;
	}
//    return -1;
    
	if(rr->pts_base == 0){
		rr->pts_base = pts;
	}

	if(pts < rr->pts_base){
		dbgw("drop audio frame (pts=%lld)\n", (long long) pts);
		return 0;
	}
	pts = pts - rr->pts_base;


//	memset(buf + length, 0, FF_INPUT_BUFFER_PADDING_SIZE);

	AVPacket packet;
	av_init_packet(&packet);
	packet.stream_index = rr->aStream->index;
	packet.data = buf;
	packet.size = length;
    AVRational time_base = rr->aStream->time_base;
    pts = (double)(pts*1000)/(double)(av_q2d(time_base)*AV_TIME_BASE);
	packet.dts = pts;
	packet.pts = pts;
//	dbgi("write audio packet: size=%d, pts=%lld\n", packet.size, (long long)packet.pts);
	if(av_interleaved_write_frame(rr->fctx, &packet) < 0) {
		//dbge("Error writing audio frame to file...\n");
	}
	return 0;
}

int rr_process_pcm(ravrecord_t rr, int64_t pts, unsigned char * buf, int length){
    
    if(!rr->audio_encoder_context) {
        return -1;
    }
    
	// pts = rr->audio_next_pts;
	
	int remains = 0;
	int copy_bytes = 0;
	// int nb_samples = length / 2 / rr->audio_encoder_context->channels;

	if(rr->pts_base == 0){
		rr->pts_base = pts;
	}
	if(pts < rr->pts_base){
		dbgw("drop audio frame (pts=%lld)\n", (long long) pts);
		return 0;
	}
	pts = pts - rr->pts_base;

	if(rr->pcm_length ==0){
		rr->audio_pts = pts;
	}

	if(rr->pcm_frame_in_bytes > 0){
		if((rr->pcm_length + length) < rr->pcm_frame_in_bytes){
			memcpy(rr->audio_pcm_buf+rr->pcm_length, buf, length);
			rr->pcm_length += length;
			//dbgi("short pcm, copy bytes %d\n", length);
			return 0;
		}
		copy_bytes = rr->pcm_frame_in_bytes-rr->pcm_length;
		memcpy(rr->audio_pcm_buf+rr->pcm_length, buf, copy_bytes);
		rr->pcm_frame->data[0] = rr->audio_pcm_buf;
		rr->pcm_frame->nb_samples = rr->pcm_frame_in_bytes/2/rr->audio_encoder_context->channels;
		remains = length - copy_bytes;
		rr->pcm_length = 0;
		//dbgi("audio: copy_bytes=%d, remains=%d\n", copy_bytes, remains);
	}else{
        if(rr->pcm_frame == NULL)
         {
            dbge("create audio avcodec failed !!!");
            return 0;
         }
        rr->pcm_frame->data[0] = buf;
		rr->pcm_frame->nb_samples = length / 2 / rr->audio_encoder_context->channels;
	}



	AVPacket packet;
	av_init_packet(&packet);
	packet.data = rr->audio_encoded_buf;
	packet.size = 48000;
	packet.stream_index = rr->aStream->index;
	// rr->pcm_frame->data[0] = buf;
	// rr->pcm_frame->nb_samples = length / 2 / rr->audio_encoder_context->channels;

//	int pcm_format = rr->audio_encoder_context->sample_fmt;
	// if(pcm_format == AV_SAMPLE_FMT_S16P && rr->audio_encoder_context->channels == 2){
	// 	// rr->pcm_frame->linesize[0] = rr->pcm_frame->nb_samples*2;
	// 	rr->pcm_frame->data[1] = rr->pcm_frame->data[0] + rr->pcm_frame->nb_samples*2;
	// 	// rr->pcm_frame->linesize[1] = rr->pcm_frame->linesize[0];
	// }else{
	// 	rr->pcm_frame->linesize[0] = rr->pcm_frame->nb_samples*2*rr->audio_encoder_context->channels;
	// }
	rr->pcm_frame->linesize[0] = rr->pcm_frame->nb_samples*2*rr->audio_encoder_context->channels;

	rr->pcm_frame->format = rr->audio_encoder_context->sample_fmt;
	rr->pcm_frame->channel_layout = rr->audio_encoder_context->channel_layout;
	rr->pcm_frame->sample_rate = rr->audio_encoder_context->sample_rate;
	rr->pcm_frame->channels = rr->audio_encoder_context->channels;
	rr->pcm_frame->pts = rr->audio_pts * rr->audio_encoder_context->sample_rate / 1000;

	rr->audio_next_pts += rr->pcm_frame->nb_samples;

	// dbgi("rr->pcm_frame->nb_samples=%d\n", rr->pcm_frame->nb_samples);
	// dbgi("rr->pcm_frame->format=%d\n", rr->pcm_frame->format);
	// dbgi("rr->pcm_frame->channel_layout=%d\n", (int)rr->pcm_frame->channel_layout);
	// dbgi("rr->pcm_frame->sample_rate=%d\n", rr->pcm_frame->sample_rate);
	// dbgi("rr->pcm_frame->channels=%d\n", rr->pcm_frame->channels);
	// dbgi("rr->pcm_frame->pts=%lld\n", (long long)rr->pcm_frame->pts);

	

	int got_frame=0;
	int ret = avcodec_encode_audio2(rr->audio_encoder_context, &packet, rr->pcm_frame, &got_frame);
	if(ret < 0){
		dbge("Error encoding audio frame ...\n");
		return -1;
	}

	
	if(remains > 0){
		memcpy(rr->audio_pcm_buf, buf+copy_bytes, remains);
		rr->pcm_length = remains;
		rr->audio_pts = pts;
	}

	if(got_frame){
       AVRational time_base = rr->aStream->time_base;
       pts = (double)(pts*1000)/(double)(av_q2d(time_base)*AV_TIME_BASE);
                
		packet.dts = pts/*rr->audio_pts*/;
		packet.pts = pts/*rr->audio_pts*/;
		packet.stream_index = rr->aStream->index;

	//	dbgi("write audio packet: size=%d, pts=%lld\n", packet.size, (long long)packet.pts);
		if(av_interleaved_write_frame(rr->fctx, &packet) < 0) {
			dbge("Error writing audio frame to file...\n");
		}
	}else{
		dbgi("no audio packet\n");
	}

	return 0;
}

