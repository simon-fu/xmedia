
#include "FFMpegFramework.hpp"
#include "spdlog/fmt/bundled/format.h"

#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)

// NOT used
class FFContainerWriter{
public:
    
    virtual ~FFContainerWriter(){
        close();
    }
    
    int open(const std::string& output_file_name,
             AVCodecID video_codec_id, int width, int height, int fps,
             AVCodecID audio_codec_id, int audio_samplerate, int audio_channels){
        int ret = 0;
        do{
            fctx_ = avformat_alloc_context();
            if(fctx_ == NULL) {
                odbge("error allocating context");
                ret = -1;
                break;
            }
            check_first_intra_frame_ = true;
            fctx_->oformat = av_guess_format(NULL, output_file_name.c_str(), NULL);
            if(fctx_->oformat == NULL) {
                odbge("Error guessing format");
                ret = -1;
                break;
            }
            snprintf(fctx_->filename, sizeof(fctx_->filename), "%s", output_file_name.c_str());
            //         if (fctx_->flags & AVFMT_GLOBALHEADER)
            //             vStream_->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            
            if(video_codec_id != AV_CODEC_ID_NONE ){
                vStream_ = new_video_stream(fctx_, video_codec_id, width, height, fps, nullptr, 0);
            }
            
            if(audio_codec_id != AV_CODEC_ID_NONE ){
                aStream_ = new_audio_stream(fctx_, audio_codec_id, audio_samplerate, audio_channels, 24000);
            }
            
            if(int err = avio_open(&fctx_->pb, fctx_->filename, AVIO_FLAG_WRITE) < 0) {
                odbge("Error opening file for output, %d\n", err);
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
            if(avformat_write_header(fctx_, &opts) < 0) {
                odbge("Error writing header\n");
                ret = -1;
                break;
            }
            
            //            if (fctx_->pb->seekable) {
            //                odbgi("seekable");
            //            } else {
            //                odbgi("not seekable");
            //            }
            
            if (opts) {
                AVDictionaryEntry* t = av_dict_get(opts, "", t, 0);
                while (t) {
                    odbge("invalid opt `%s` found", t->key);
                    t = av_dict_get(opts, "", t, 0);
                }
            }
            write_header_ = 1;
            
            ret = 0;
            
        }while(0);
        
        if(ret){
            close();
        }
        return ret;
    }
    
    void close(){
        if(fctx_ != NULL && write_header_){
            av_write_trailer(fctx_);
        }
        
        if(audio_encoder_context_) {
            avcodec_free_context(&audio_encoder_context_);
            audio_encoder_context_ = nullptr;
        }
        
        if (pcm_frame_) {
            av_frame_free(&pcm_frame_);
            pcm_frame_ = nullptr;
        }
        
        if(fctx_ != NULL) {
            avio_close(fctx_->pb);
            fctx_->pb = nullptr;
            avformat_free_context(fctx_);
            fctx_ = nullptr;
        }
    }
    
    int writeVideo(int64_t pts, unsigned char * buf, int length, bool is_key_frame){
        
        if(!vStream_){
            return -1;
        }
        
        if(pts_base_ == 0){
            pts_base_ = pts;
        }
        
        pts = pts - pts_base_;
        if(pts < 0){
            pts = 0;
        }
        
        if (check_first_intra_frame_) {
            if (!is_key_frame) {
                //dbge("discard inter frame without preceeding intra frame");
                return -1;
            } else {
                check_first_intra_frame_ = false;
            }
        }
        
        AVPacket packet;
        av_init_packet(&packet);
        packet.stream_index = vStream_->index;
        packet.data = buf;
        packet.size = length;
        
        AVRational& time_base = vStream_->time_base;
        pts = pts * time_base.den/time_base.num/1000;
        
        if (is_key_frame){
            packet.flags |= AV_PKT_FLAG_KEY;
        }
        
        packet.dts = pts;
        packet.pts = pts;
        if(int err = av_interleaved_write_frame(fctx_, &packet) < 0) {
            odbge("write one video frame to file %s failed", fctx_->filename);
        }
        return 0;
    }
    
    int writeAudio(int64_t pts, unsigned char * buf, int length){
        
        if(!aStream_){
            return -1;
        }
        
        if(pts_base_ == 0){
            pts_base_ = pts;
        }
        
        if(pts < pts_base_){
            //dbgw("drop audio frame (pts=%lld)\n", (long long) pts);
            return -1;
        }
        pts = pts - pts_base_;
        //printf("write audio: pts=%lld\n", pts);
        
        AVPacket packet;
        av_init_packet(&packet);
        packet.stream_index = aStream_->index;
        packet.data = buf;
        packet.size = length;
        
        //AVRational time_base = rr->aStream->time_base;
        //pts = (double)(pts*1000)/(double)(av_q2d(time_base)*AV_TIME_BASE);
        
        AVRational& time_base = aStream_->time_base;
        pts = pts * time_base.den/time_base.num/1000;
        
        packet.dts = pts;
        packet.pts = pts;
        if(av_interleaved_write_frame(fctx_, &packet) < 0) {
            odbge("write one audio frame to file %s failed\n", fctx_->filename);
        }
        return 0;
    }
    
    
private:
    static
    AVStream * new_video_stream(AVFormatContext * fctx, enum AVCodecID codecid, int max_width, int max_height, int fps,unsigned char *sps_pps_buffer, int  sps_pps_length){
        int ret = 0;
        AVStream *stream = NULL;
        do{
            stream = avformat_new_stream(fctx, 0);
            if(stream == NULL) {
                ret = -1;
                NERROR_FMT_SET(ret, "avformat_new_stream video fail");
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
            
            if(codecid == AV_CODEC_ID_H264){
                if(sps_pps_buffer && sps_pps_length > 0){
                    stream->codecpar->extradata = new uint8_t[sps_pps_length];
                    memcpy(stream->codecpar->extradata, sps_pps_buffer,sps_pps_length);
                    stream->codecpar->extradata_size = sps_pps_length;
                }
            }
        }while(0);
        
        return stream;
        
    }
    
    // todo: audio transcoding part is broken
    static
    AVStream * new_audio_stream(AVFormatContext * fctx, enum AVCodecID codecid, int sample_rate, int channels, int bit_rate){
        int ret = 0;
        AVStream *stream = NULL;
        do{
            const AVCodec *codec = avcodec_find_encoder(codecid);
            stream = avformat_new_stream(fctx, codec);
            if(stream == NULL) {
                ret = -1;
                NERROR_FMT_SET(ret, "avformat_new_stream audio fail");
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
            stream->codecpar->frame_size = 1024; // TODO:
            
            odbgi("audio-stream: ar=%d, ch=[%d,0x%08llX], bitrate=%lld, frmsz=%d"
                  , stream->codecpar->sample_rate
                  , stream->codecpar->channels
                  , stream->codecpar->channel_layout
                  , stream->codecpar->bit_rate
                  , stream->codecpar->frame_size);
            
            //        if (fctx->flags & AVFMT_GLOBALHEADER)
            //            stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            
            ret = 0;
            
        }while(0);
        
        if(ret){
            // TODO:
        }
        
        return stream;
        
    }
private:
    AVFormatContext *fctx_ = nullptr;
    int write_header_ = 0;
    AVStream *vStream_ = nullptr;
    AVStream *aStream_ = nullptr;
    AVCodecContext * audio_encoder_context_ = nullptr;
    AVFrame * pcm_frame_ = nullptr;
    int64_t audio_next_pts_ = 0;
    int64_t audio_pts_ = 0;
    int64_t pts_base_ = 0;
    //enum AVCodecID video_codecid;
    bool check_first_intra_frame_ = false;
};

bool FFMediaConfigLessIndex(const FFMediaConfig & m1, const FFMediaConfig & m2) {
    return m1.getIndex() < m2.getIndex();
}

