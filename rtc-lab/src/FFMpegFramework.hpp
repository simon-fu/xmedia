

#ifndef FFMpegFramework_hpp
#define FFMpegFramework_hpp

#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include "NUtil.hpp"

extern "C"{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavdevice/avdevice.h"
#include "libavutil/imgutils.h"
//#include "libavutil/samplefmt.h"
}

class FFTrack{
protected:
    std::string          name_ = "FFTrack";
    int                  index_ = -1;
    AVStream *           stream_ = NULL;
    AVCodecContext     * codecCtx_ = NULL;
    AVFrame            * avFrame_ = NULL;
    AVFrame            * lastFrame_ = NULL;
public:
    
    virtual ~FFTrack(){
        close();
    }
    
    virtual int getIndex(){
        return index_;
    }
    
    virtual AVMediaType getMediaType(){
        if(!stream_) return AVMEDIA_TYPE_UNKNOWN;
        return stream_->codecpar->codec_type;
    }
    
    virtual AVFrame * getLastFrame(){
        return lastFrame_;
    }
    
    virtual void setStream(int index, AVStream * stream){
        index_ = index;
        stream_ = stream;
    }
    
    virtual AVRational getTimebase(){
        if(this->codecCtx_){
            return this->codecCtx_->time_base;
        }else{
            return (AVRational){0, 1};
        }
    }
    
    virtual int open(const std::map<std::string, std::string> codecOpt){
        int ret = -1;
        do{
            AVCodecID codec_id = stream_->codecpar->codec_id;
            AVCodec * pCodec = avcodec_find_decoder(codec_id);
            if(!pCodec){
                //odbge("fail to find codec [%d]", codec_id);
                ret = -31;
                break;
            }
            
            AVDictionary* codecDict = NULL;
            for(auto o : codecOpt){
                //odbgi("stream[%d] codec: dict[%s]=[%s]", index_, o.first.c_str(), o.second.c_str());
                av_dict_set(&codecDict, o.first.c_str(), o.second.c_str(), 0);
            }
            
            this->codecCtx_ = avcodec_alloc_context3(pCodec);
            if(!this->codecCtx_){
                //odbge("fail to alloc codec context, [%d][%s]", pCodec->id, pCodec->name);
                ret = -41;
                break;
            }
            if(avcodec_open2(this->codecCtx_, pCodec, &codecDict)<0){
                //odbge("fail to open codec context, [%d][%s]", pCodec->id, pCodec->name);
                ret = -51;
                break;
            }
            
            this->avFrame_ = av_frame_alloc();
            ret = 0;
        }while(0);
        
        if(ret){
            this->close();
        }
        return ret;
    }
    
    virtual void close(){
        if(this->avFrame_){
            av_frame_free(&this->avFrame_);
            this->avFrame_ = nullptr;
        }
        if(this->codecCtx_){
            avcodec_close(this->codecCtx_);
            avcodec_free_context(&this->codecCtx_);
            this->codecCtx_ = nullptr;
        }
        lastFrame_ = NULL;
    }
    
    virtual int pullFrame(){
        int ret = avcodec_receive_frame(codecCtx_, avFrame_);
        //av_frame_unref(avFrame_); // TODO: check calling av_frame_unref?
        lastFrame_ = ret == 0 ? avFrame_ : NULL;
        return ret;
    }
    
    virtual int pushPacket(AVPacket* packet){
        return avcodec_send_packet(codecCtx_, packet);
    }
}; // FFTrack

class FFAudioTrack : public FFTrack {
    int samplerate_ = -1;
    int channels_ = -1;
    AVSampleFormat sampleFormat_ = AV_SAMPLE_FMT_NONE;
    int          frameSize_ = 0;
    SwrContext * audioConverCtx_ = NULL;
    AVFrame    * dataFrame_ = NULL;
    uint8_t    * dataBuffer_ = NULL;
    size_t       dataSize_ = 0;
    
public:
    FFAudioTrack(int samplerate, int channels, AVSampleFormat sampleFormat, int frameSize = 0)
    : samplerate_(samplerate), channels_(channels), sampleFormat_(sampleFormat), frameSize_(frameSize){
        
    }
    
    virtual int open(const std::map<std::string, std::string> codecOpt) override{
        int ret = -1;
        do{
            std::map<std::string, std::string> trackCodecOpt = codecOpt;
            AVCodecParameters * param = stream_->codecpar;
            char buf[64];
            if(codecOpt.find("ac") == codecOpt.end()){
                sprintf(buf, "%d", param->channels);
                trackCodecOpt["ac"] = buf;
            }
            if(codecOpt.find("ar") == codecOpt.end()){
                sprintf(buf, "%d", param->sample_rate);
                trackCodecOpt["ar"] = buf;
            }
            
            ret = FFTrack::open(trackCodecOpt);
            if(ret){
                break;
            }
            
            if(samplerate_ < 0){
                samplerate_ = codecCtx_->sample_rate;
            }
            if(channels_ < 0){
                channels_ = codecCtx_->channels;
            }
            if(sampleFormat_ < 0){
                sampleFormat_ = codecCtx_->sample_fmt;
            }
            if(frameSize_ <= 0){
                // codecCtx_->frame_size maybe 0
                frameSize_ = codecCtx_->frame_size;
            }
            
            //AVSampleFormat origin_sample_fmt = (AVSampleFormat)param->format;
            AVSampleFormat origin_sample_fmt = codecCtx_->sample_fmt;
            if(samplerate_ != param->sample_rate
               || channels_ != param->channels
               || sampleFormat_ != origin_sample_fmt){
                
                audioConverCtx_ = swr_alloc_set_opts(
                                                     NULL,
                                                     av_get_default_channel_layout(channels_),
                                                     sampleFormat_,
                                                     samplerate_,
                                                     av_get_default_channel_layout(param->channels),
                                                     origin_sample_fmt,
                                                     param->sample_rate,
                                                     0, NULL);
                
                if(!audioConverCtx_){
                    ret = -61;
                    break;
                }
                swr_init(audioConverCtx_);
                dataFrame_ = av_frame_alloc();
                dataFrame_->sample_rate = samplerate_;
                dataFrame_->format = sampleFormat_;
                dataFrame_->channels = channels_;
                dataFrame_->channel_layout = av_get_default_channel_layout(channels_);
                // alloc buffer
                ret = reallocBuffer(frameSize_);
                
            }
            
        }while(0);
        return ret;
    }
    
    virtual void close() override{
        FFTrack::close();
        if(audioConverCtx_){
            if(swr_is_initialized(audioConverCtx_)){
                swr_close(audioConverCtx_);
            }
            swr_free(&audioConverCtx_);
        }
        
        if(dataFrame_){
            av_frame_free(&dataFrame_);
            dataFrame_ = nullptr;
        }
        
        if(dataBuffer_){
            av_freep(&dataBuffer_);
        }
    }
    
    int reallocBuffer(int frameSize){
        // alloc buffer
        int ret = 0;
        if(frameSize > 0){
            
            if(frameSize > frameSize_){
                dataFrame_->nb_samples = frameSize;
                dataFrame_->format = sampleFormat_;
                dataFrame_->channels = channels_;
                
//                int sz = av_samples_get_buffer_size(NULL, channels_, frameSize, sampleFormat_, 1);
//                if(dataBuffer_){
//                    av_freep(&dataBuffer_);
//                }
//                dataSize_ = sz;
//                dataBuffer_ = (uint8_t *) av_malloc(sz);
//                ret = avcodec_fill_audio_frame(dataFrame_, dataFrame_->channels, (AVSampleFormat)dataFrame_->format, (const uint8_t*)dataBuffer_, (int)sz, 0);
//                if(ret >= 0){
//                    ret = 0;
//                }
                
                if(frameSize_ > 0){
                    av_frame_unref(dataFrame_);
                }
                av_frame_get_buffer(dataFrame_, 0);
                
                frameSize_ = frameSize;
            }
        }
        return ret;
    }
    
    virtual int pullFrame() override{
        int ret = FFTrack::pullFrame();
        if(ret) return ret;
        
        if(audioConverCtx_){
            if(avFrame_->nb_samples > frameSize_){
                this->reallocBuffer(avFrame_->nb_samples);
            }
            dataFrame_->nb_samples = swr_convert(audioConverCtx_, dataFrame_->data, frameSize_,
                                                 (const uint8_t**)avFrame_->data, avFrame_->nb_samples);
            
            dataFrame_->pkt_size = av_samples_get_buffer_size(dataFrame_->linesize, dataFrame_->channels,
                                                              dataFrame_->nb_samples, sampleFormat_, 1);
            
            dataFrame_->pts = avFrame_->pts;
            //odbgi("input nb_samples=%d, output nb_samples=%d pkt_size=%d", avFrame_->nb_samples, dataFrame_->nb_samples, dataFrame_->pkt_size);
            
            lastFrame_ = dataFrame_;
        }else{
            if(lastFrame_){
                lastFrame_->channels = channels_;
                lastFrame_->channel_layout = av_get_default_channel_layout(channels_);
            }
        }
        return ret;
    }
    
    int getSamplerate(){
        return samplerate_;
    }
    
    int getChannels(){
        return channels_;
    }
    
    AVSampleFormat getSampleFormat(){
        return sampleFormat_;
    }
    
}; // FFAudioTrack

class FFVideoTrack : public FFTrack {
    AVPixelFormat outputFormat_ = AV_PIX_FMT_YUV420P;
    struct SwsContext  *imgConvertCtx_ = NULL;
    AVFrame    * avFrameYUV_ = NULL;
    uint8_t    * imageBuffer_ = NULL;
    size_t     imageSize_ = 0;
public:
    FFVideoTrack(AVPixelFormat outputFormat) : outputFormat_(outputFormat){
        
    }
    virtual int open(const std::map<std::string, std::string> codecOpt) override{
        int ret = -1;
        do{
            std::map<std::string, std::string> trackCodecOpt = codecOpt;
            AVCodecParameters * param = stream_->codecpar;
            char buf[64];
            if(codecOpt.find("video_size") == codecOpt.end()){
                sprintf(buf, "%dx%d", param->width, param->height);
                trackCodecOpt["video_size"] = buf;
            }
            if(codecOpt.find("pixel_format") == codecOpt.end()){
                sprintf(buf, "%d", param->format);
                trackCodecOpt["pixel_format"] = buf;
            }
            //if(codecOpt.find("framerate") == codecOpt.end()){
            //    av_dict_set_int(&codecDict, "framerate", 30, 0);
            //}
            
            ret = FFTrack::open(trackCodecOpt);
            if(ret){
                break;
            }
            
            if(outputFormat_ < 0){
                outputFormat_ = this->codecCtx_->pix_fmt;
            }
            
            if(this->codecCtx_->pix_fmt != outputFormat_){
                imgConvertCtx_ = sws_getContext(this->codecCtx_->width, this->codecCtx_->height, this->codecCtx_->pix_fmt
                                                , this->codecCtx_->width, this->codecCtx_->height, outputFormat_
                                                , SWS_BICUBIC, NULL, NULL, NULL);
                if(!imgConvertCtx_){
                    ret = -61;
                    break;
                }
                
                // alloc yuv buffer
                imageSize_ = av_image_get_buffer_size(outputFormat_, this->codecCtx_->width, this->codecCtx_->height, 1);
                imageBuffer_ = (uint8_t *) av_malloc(imageSize_);
                
                // alloc yuv frame with yuv buffer
                avFrameYUV_ = av_frame_alloc();
                avFrameYUV_->width = this->codecCtx_->width;
                avFrameYUV_->height = this->codecCtx_->height;
                avFrameYUV_->format = outputFormat_;
                av_image_fill_arrays(avFrameYUV_->data, avFrameYUV_->linesize, imageBuffer_, outputFormat_, this->codecCtx_->width, this->codecCtx_->height, 1);
            }
        }while(0);
        return ret;
    }
    
    virtual int  pullFrame() override{
        int ret = FFTrack::pullFrame();
        if(ret) return ret;
        if(imgConvertCtx_){
            sws_scale(imgConvertCtx_, (const unsigned char* const*)this->avFrame_->data, this->avFrame_->linesize
                      , 0, this->avFrame_->height
                      , avFrameYUV_->data, avFrameYUV_->linesize);
            avFrameYUV_->width = this->avFrame_->width;
            avFrameYUV_->height = this->avFrame_->height;
            avFrameYUV_->pkt_size = (int)imageSize_;
            avFrameYUV_->pts = this->avFrame_->pts;
            avFrameYUV_->pkt_dts = this->avFrame_->pkt_dts;
            lastFrame_ = avFrameYUV_;
        }
        return ret;
    }
    
    virtual void close() override{
        FFTrack::close();
        
        if(imgConvertCtx_){
            sws_freeContext(imgConvertCtx_);
            imgConvertCtx_ = NULL;
        }
        
        if(avFrameYUV_){
            av_frame_free(&avFrameYUV_);
        }
        
        if(imageBuffer_){
            av_freep(&imageBuffer_);
        }
    }
    
    int getWidth(){
        return codecCtx_ ? codecCtx_->width : -1;
    }
    
    int getHeight(){
        return codecCtx_ ? codecCtx_->height : -1;
    }
    
    AVPixelFormat getPixelFormat(){
        return codecCtx_ ? outputFormat_ : AV_PIX_FMT_NONE;
    }
    
}; // FFVideoTrack

class FFContainerReader{
    
protected:
    std::string             name_;
    AVFormatContext        *formatCtx_ = NULL;
    AVPacket               *avPacket_ = NULL;
    std::vector<FFTrack*>   tracks_;
    FFTrack *               lastTrack_ = NULL;
    AVDictionary*           containerDict_ = NULL;
public:
    FFContainerReader(const std::string& name):name_(name){
        
    }
    
    virtual ~FFContainerReader(){
        close();
    }
    
    int setVideoOptions(int width, int height, int framerate, const std::string& optPixelFmt){
        char buf[64];
        if(width > 0 && height > 0){
            sprintf(buf, "%dx%d", width, height);
            av_dict_set(&containerDict_, "video_size", buf, 0);
        }
        if(framerate > 0){
            av_dict_set_int(&containerDict_, "framerate", framerate, 0);
        }
        if(!optPixelFmt.empty()){
            av_dict_set(&containerDict_, "pixel_format", optPixelFmt.c_str(), 0);
        }
        return 0;
    }
    
    int open( const std::string& containerFormat
             , const std::string& url){
        return open(containerFormat, url, std::map<std::string, std::string>());
    }
    
    int open( const std::string& containerFormat
             , const std::string& url
             , const std::map<std::string, std::string> cantainerOpt){
        if(formatCtx_){
            // already opened
            return 0;
        }
        
        int ret = -1;
        do{
            formatCtx_ = avformat_alloc_context();
            AVInputFormat *ifmt = NULL;
            if(!containerFormat.empty()){
                ifmt = av_find_input_format(containerFormat.c_str());
                if(!ifmt){
                    //odbge("fail to find format [%s]", containerFormat.c_str());
                    ret = -11;
                    break;
                }
            }
            for(auto o : cantainerOpt){
                av_dict_set(&containerDict_, o.first.c_str(), o.second.c_str(), 0);
            }
            ret = avformat_open_input(&formatCtx_, url.c_str(), ifmt, &containerDict_) ;
            if(ret != 0){
                //odbge("fail to open url=[%s], format=[%s], err=[%d]-[%s]", url.c_str(), containerFormat.c_str(), ret, av_err2str(ret));
                break;
            }
            ret = avformat_find_stream_info(formatCtx_, NULL);
            if(ret < 0){
                //odbge("fail to find stream, [%d]-[%s]", ret, av_err2str(ret));
                break;
            }
            
            for(int i=0; i< formatCtx_->nb_streams; i++){
                tracks_.push_back(NULL);
            }
            
            avPacket_ = (AVPacket *) av_malloc(sizeof(AVPacket));
            
            ret = 0;
        }while(0);
        
        if(ret){
            this->close();
        }
        return ret;
    }
    
    void close(){
        for(int i = 0; i < tracks_.size(); ++i){
            this->closeTrack(tracks_[i]);
            tracks_[i] = NULL;
        }
        tracks_.clear();
        if(avPacket_){
            av_freep(&avPacket_);
        }
        
        if(formatCtx_){
            avformat_close_input(&formatCtx_);
            //avformat_free_context(formatCtx_);
            formatCtx_ = NULL;
        }
        lastTrack_ = NULL;
    }
    
    FFVideoTrack * openVideoTrack(AVPixelFormat outputFormat){
        AVMediaType mediaType = AVMEDIA_TYPE_VIDEO;
        std::map<std::string, std::string> codecOpt;
        int ret = -1;
        FFVideoTrack * track = NULL;
        int index = -1;
        do{
            
            for(int i=0; i< formatCtx_->nb_streams; i++){
                AVMediaType mtype = formatCtx_->streams[i]->codecpar->codec_type;
                if(mtype == mediaType){
                    index = i;
                    break;
                }
            }
            
            if(index < 0){
                //odbge("fail to find a media type %d", mediaType);
                ret = -21;
                break;
            }
            
            if(tracks_[index]){
                // already opened
                ret = 0;
                break;
            }
            
            track = new FFVideoTrack(outputFormat);
            track->setStream(index, formatCtx_->streams[index]);
            ret = track->open(codecOpt);
            if(ret){
                break;
            }
            tracks_[index] = track;
            ret = 0;
        }while(0);
        
        if(ret){
            this->closeTrack(track);
            track = NULL;
            index = -1;
        }
        return track;
    }
    
    FFAudioTrack * openAudioTrack(int samplerate, int channels, AVSampleFormat sampleFormat){
        AVMediaType mediaType = AVMEDIA_TYPE_AUDIO;
        std::map<std::string, std::string> codecOpt;
        int ret = -1;
        FFAudioTrack * track = NULL;
        int index = -1;
        do{
            
            for(int i=0; i< formatCtx_->nb_streams; i++){
                AVMediaType mtype = formatCtx_->streams[i]->codecpar->codec_type;
                if(mtype == mediaType){
                    index = i;
                    break;
                }
            }
            
            if(index < 0){
                //odbge("fail to find a media type %d", mediaType);
                ret = -21;
                break;
            }
            
            if(tracks_[index]){
                // already opened
                ret = 0;
                break;
            }
            
            track = new FFAudioTrack(samplerate, channels, sampleFormat);
            track->setStream(index, formatCtx_->streams[index]);
            ret = track->open(codecOpt);
            if(ret){
                break;
            }
            tracks_[index] = track;
            ret = 0;
        }while(0);
        
        if(ret){
            this->closeTrack(track);
            track = NULL;
            index = -1;
        }
        return track;
    }
    
    void closeTrack(FFTrack *track){
        if(!track) return;
        for(int i = 0; i < tracks_.size(); ++i){
            if(tracks_[i] == track){
                track->close();
                delete track;
                tracks_[i] = NULL;
                break;
            }
        }
    }
    
    FFTrack * getTrack(int index){
        if(index < 0 || index >= tracks_.size()){
            return NULL;
        }
        return tracks_[index];
    }
    
    // may block if no frame available
    FFTrack * readNext(){
        int ret = 0;
        if(lastTrack_){
            ret = lastTrack_->pullFrame();
            if(ret == 0){
                return lastTrack_;
            }
        }
        lastTrack_ = NULL;
        while(1){
            int ret = av_read_frame(formatCtx_, avPacket_);
            if(ret < 0){
                //odbge("fail to read packet, [%d]-[%s]", ret, av_err2str(ret));
                break;
            }
            if(avPacket_->stream_index >= 0 && avPacket_->stream_index < tracks_.size()){
                FFTrack * track = tracks_[avPacket_->stream_index];
                if(track){
                    track->pushPacket(avPacket_);
                    ret = track->pullFrame();
                    if(ret != AVERROR(EAGAIN)){
                        if(ret == 0){
                            lastTrack_ = track;
                        }
                        break;
                    }
                }
            }
        }
        return lastTrack_;
    }
    
}; // FFContainerReader


class FFPTSConverter{
public:
    FFPTSConverter():FFPTSConverter((AVRational){1, 1000}, (AVRational){1, 1000}){
        
    }
    
    FFPTSConverter(const AVRational& src, const AVRational& dst)
    : src_(src), dst_(dst){
        
    }
    
    void setSrcBase(const AVRational& src){
        src_ = src;
    }
    
    void setDstBase(const AVRational& dst){
        dst_ = dst;
    }
    
    const AVRational& src()const{
        return src_;
    }
    
    const AVRational& dst()const{
        return dst_;
    }
    
    int64_t convert(int64_t pts){
        pts = pts * dst_.den/dst_.num/src_.den/src_.num;
        return pts;
    }
private:
    AVRational src_;
    AVRational dst_;
}; // FFPTSConverter

class Int64Relative{
public:
    class Offset{
    public:
        int64_t offset(int64_t pts){
            if(!validbase_){
                validbase_ = true;
                base_ = pts;
            }
            pts = pts - base_;
            return pts;
        }
    private:
        bool validbase_ = false;
        int64_t base_ = 0;
    };
    
    class Delta{
    public:
        int64_t delta(int64_t pts){
            if(!validlast_){
                validlast_ = true;
                last_ = pts;
            }
            int64_t delta = pts - last_;
            last_ = pts;
            return delta;
        }
    private:
        bool validlast_ = false;
        int64_t last_ = 0;
    };
    
public:
    
    int64_t offset(int64_t pts){
        return offset_.offset(pts);
    }
    
    int64_t delta(int64_t pts){
        return delta_.delta(pts);
    }
    
private:
    Offset offset_;
    Delta delta_;
}; // Int64Relative

class Int64Delta{
public:
    int64_t delta(int64_t pts){
        if(first_){
            first_ = false;
            last_ = pts;
        }
        pts = pts - last_;
        last_ = pts;
        return pts;
    }
private:
    bool first_ = true;
    int64_t last_ = 0;
};

class FFUtil{
public:
    static bool validTimeBase(const AVRational& time_base){
        return (time_base.num > 0 && time_base.den > 0);
    }
    
    static void assignCommon(const AVStream * stream, AVCodecContext * ctx){
        ctx->codec_type = stream->codecpar->codec_type;
        ctx->codec_id = stream->codecpar->codec_id;
        ctx->time_base = stream->time_base;
        if(ctx->codec_type == AVMEDIA_TYPE_AUDIO){
            ctx->sample_rate = stream->codecpar->sample_rate;
            ctx->sample_fmt = (AVSampleFormat)stream->codecpar->format;
            ctx->frame_size = stream->codecpar->frame_size;
            ctx->channel_layout = stream->codecpar->channel_layout;
            ctx->channels = av_get_channel_layout_nb_channels(ctx->channel_layout);
        }else if(ctx->codec_type == AVMEDIA_TYPE_VIDEO){
            ctx->pix_fmt = (AVPixelFormat)stream->codecpar->format;;
            ctx->width = stream->codecpar->width;
            ctx->height = stream->codecpar->height;
            
            if(validTimeBase(stream->avg_frame_rate)){
                ctx->framerate = stream->avg_frame_rate;
            }else if(validTimeBase(stream->r_frame_rate)){
                ctx->framerate = stream->r_frame_rate;
            }
        }
    }
    
    static void assignDecode(const AVStream * stream, AVCodecContext * ctx){
        assignCommon(stream, ctx);
        if(ctx->codec_type == AVMEDIA_TYPE_AUDIO){
            ctx->request_sample_fmt = (AVSampleFormat)stream->codecpar->format;
            ctx->request_channel_layout = stream->codecpar->channel_layout;
            ctx->channels = av_get_channel_layout_nb_channels(ctx->channel_layout);
        }else if(ctx->codec_type == AVMEDIA_TYPE_VIDEO){

        }
    }
}; // FFUtil

struct FFSampleConfig{
private:
    AVSampleFormat sample_fmt;
    int sample_rate ;
    uint64_t channel_layout ;
    int frame_size;
    
public:
    FFSampleConfig(){
        reset();
    }
    
    void setFormat(AVSampleFormat fmt){
        sample_fmt = fmt;
    }
    
    void setSampleRate(int r){
        sample_rate = r;
    }
    
    void setChLayout(uint64_t layout){
        channel_layout = layout;
    }
    
    void setFrameSize(int sz){
        frame_size = sz;
    }
    
    AVSampleFormat getFormat()const{
        return sample_fmt;
    }
    
    AVSampleFormat getFormat(AVSampleFormat default_val)const{
        return sample_fmt > AV_SAMPLE_FMT_NONE ? sample_fmt : default_val;
    }
    
    int getFrameSize()const{
        return frame_size ;
    }
    
    int getFrameSize(int default_val)const{
        return frame_size > 0 ? frame_size : default_val;
    }
    
    int getChannels()const{
        return av_get_channel_layout_nb_channels(channel_layout);
    }
    
    uint64_t getChLayout()const{
        return channel_layout;
    }
    
    uint64_t getChLayout(uint64_t default_val)const{
        return channel_layout > 0 ? channel_layout : default_val;
    }
    
    int getSampleRate()const{
        return sample_rate;
    }
    
    int getSampleRate(int default_val)const{
        return sample_rate > 0 ? sample_rate : default_val;
    }
    
    void reset(){
        sample_rate = -1;
        channel_layout = 0;
        sample_fmt = AV_SAMPLE_FMT_NONE;
        frame_size = 0;
    }
    
    void assign(const AVFrame* frame){
        sample_rate = frame->sample_rate;
        channel_layout = frame->channel_layout;
        sample_fmt = (AVSampleFormat)frame->format;
    }
    
    void assign(const AVStream * stream){
        assign(stream->codecpar);
    }
    
    void assign(const AVCodecParameters* codecpar){
        sample_rate = codecpar->sample_rate;
        channel_layout = codecpar->channel_layout;
        sample_fmt = (AVSampleFormat)codecpar->format;
    }
    
    void assign(const AVCodecContext * ctx){
        sample_rate = ctx->sample_rate;
        channel_layout = ctx->channel_layout;
        sample_fmt = ctx->sample_fmt;
        frame_size = ctx->frame_size;
    }
    
    void get(AVCodecContext * ctx){
        ctx->sample_rate = sample_rate;
        ctx->channel_layout = channel_layout;
        ctx->channels = getChannels();
        ctx->sample_fmt = sample_fmt;
        ctx->frame_size = frame_size;
    }
    
    bool valid()const{
        return (sample_rate > 0 && channel_layout > 0 && sample_fmt > AV_SAMPLE_FMT_NONE);
    }
    
    bool equalBase(const FFSampleConfig& other)const{
        return (sample_rate == other.sample_rate
                && channel_layout == other.channel_layout
                && sample_fmt == other.sample_fmt);
    }
    
    bool equalBase(const AVFrame* frame)const{
        return (sample_rate == frame->sample_rate
                && channel_layout == frame->channel_layout
                && (int)sample_fmt == frame->format);
    }
    
    bool equal(const FFSampleConfig& other)const{
        return (equalBase(other)
                && frame_size == other.frame_size);
    }
    
    void match(const FFSampleConfig& stdcfg){
        FFSampleConfig& cfg = *this;
        if(cfg.sample_fmt <= AV_SAMPLE_FMT_NONE){
            cfg.sample_fmt = stdcfg.sample_fmt;
        }
        if(cfg.sample_rate <= 0){
            cfg.sample_rate = stdcfg.sample_rate;
        }
        if(cfg.channel_layout == 0){
            cfg.channel_layout = stdcfg.channel_layout;
        }
        if(cfg.frame_size <= 0 && stdcfg.frame_size > 0){
            cfg.frame_size = stdcfg.frame_size;
        }
    }
}; // FFSampleConfig

struct FFImageConfig{
private:
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    int width = -1;
    int height = -1;
    int frame_rate = -1;
    
public:
    void setFormat(AVPixelFormat fmt){
        pix_fmt = fmt;
    }
    
    void setWidth(int w){
        width = w;
    }
    
    void setHeight(int h){
        height = h;
    }
    
    void setFrameRate(int fps){
        frame_rate = fps;
    }
    
    AVPixelFormat getFormat()const{
        return pix_fmt;
    }
    
    AVPixelFormat getFormat(AVPixelFormat default_val)const{
        return pix_fmt > AV_PIX_FMT_NONE ? pix_fmt : default_val;
    }
    
    int getWidth()const{
        return width;
    }
    
    int getWidth(int default_val)const{
        return width > 0 ? width : default_val;
    }
    
    int getHeight()const{
        return height;
    }
    
    int getHeight(int default_val)const{
        return height > 0 ? height : default_val;
    }
    
    int getImageSize()const{
        return width*height;
    }
    
    int getFrameRate()const{
        return frame_rate;
    }
    
    int getFrameRate(int default_value)const{
        return frame_rate > 0 ? frame_rate : default_value;
    }
    
    void assign(const AVFrame* frame){
        pix_fmt = (AVPixelFormat)frame->format;
        width = frame->width;
        height = frame->height;
    }
    
    void assign(const AVStream * stream){
        const AVCodecParameters* codecpar = stream->codecpar;
        pix_fmt = (AVPixelFormat)codecpar->format;
        width = codecpar->width;
        height = codecpar->height;
        if(stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0){
            frame_rate = stream->avg_frame_rate.den / stream->avg_frame_rate.num;
        }else if(stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0){
            frame_rate = stream->r_frame_rate.den / stream->r_frame_rate.num;
        }
    }
    
    void assign(const AVCodecContext * ctx){
        pix_fmt = ctx->pix_fmt;
        width = ctx->width;
        height = ctx->height;
        if(FFUtil::validTimeBase(ctx->framerate)){
            frame_rate = ctx->framerate.num/ctx->framerate.den;
        }
    }
    
    void get(AVCodecContext * ctx){
        ctx->pix_fmt = pix_fmt;
        ctx->width = width;
        ctx->height = height;
        if(frame_rate > 0){
            ctx->framerate = {frame_rate, 1};
        }
    }
    
    bool valid()const{
        return !(pix_fmt <= AV_PIX_FMT_NONE || width <= 0 || height <= 0);
    }
    
    bool equal(const FFImageConfig& other)const{
        if(&other == this) return true;
        return (pix_fmt == other.pix_fmt
                && width == other.width
                && height == other.height
                && frame_rate == other.frame_rate);
    }
    
    bool equal(const AVFrame* frame)const{
        if(!frame) return false;
        return ((int)pix_fmt == frame->format
                && width == frame->width
                && height == frame->height);
    }
    
    void match(const FFImageConfig& stdcfg){
        FFImageConfig& cfg = *this;
        if(cfg.pix_fmt <= AV_PIX_FMT_NONE){
            cfg.pix_fmt = stdcfg.pix_fmt;
        }
        if(cfg.width <= 0){
            cfg.width = stdcfg.width;
        }
        if(cfg.height <= 0){
            cfg.height = stdcfg.height;
        }
    }
}; // FFImageConfig

struct FFMediaConfig{
private:
    AVMediaType codec_type = AVMEDIA_TYPE_UNKNOWN;
    AVCodecID codec_id = AV_CODEC_ID_NONE;
    AVRational time_base;
    int index = -1;
    
public:
    
    FFSampleConfig audio;
    FFImageConfig video;
    
public:
    FFMediaConfig():time_base({1,1000}){
        
    }
    
    void assign(const AVStream * stream){
        codec_type = stream->codecpar->codec_type;
        codec_id = stream->codecpar->codec_id;
        time_base = stream->time_base;
        index = stream->index;
        if(stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audio.assign(stream);
        }else if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            video.assign(stream);
        }
    }
    
    void assignCommon(const AVCodecContext * ctx){
        codec_type = ctx->codec_type;
        codec_id = ctx->codec_id;
        if(codec_type == AVMEDIA_TYPE_AUDIO){
            audio.assign(ctx);
        }else if(codec_type == AVMEDIA_TYPE_VIDEO){
            video.assign(ctx);
        }
    }
    
    void assignDecode(const AVCodecContext * ctx){
        assignCommon(ctx);
        //time_base = ctx->time_base; // TODO: NOT used for decoding?
    }
    
    void assignEncode(const AVCodecContext * ctx){
        assignCommon(ctx);
        time_base = ctx->time_base;
    }
    
    void get(AVCodecContext * ctx){
        ctx->codec_type = codec_type;
        ctx->codec_id = codec_id;
        ctx->time_base = time_base;
        if(codec_type == AVMEDIA_TYPE_AUDIO){
            audio.get(ctx);
        }else if(codec_type == AVMEDIA_TYPE_VIDEO){
            video.get(ctx);
        }
    }
    
    void setCodecId(AVCodecID id){
        codec_id = id;
        // TODO: get codec type directly
        AVCodec * codec = avcodec_find_decoder(codec_id);
        if(codec){
            codec_type = codec->type;
        }else{
            codec_type = AVMEDIA_TYPE_UNKNOWN;
        }
    }
    
    AVCodecID getCodecId()const{
        return codec_id;
    }
    
    AVCodecID getCodecId(AVCodecID default_val)const{
        return codec_id > AV_CODEC_ID_NONE ? codec_id : default_val;
    }
    
    void setTimeBase(const AVRational& base){
        time_base = base;
    }
    
    const AVRational& getTimeBase()const{
        return time_base;
    }
    
    AVMediaType getCodecType()const{
        return codec_type;
    }
    
    void setIndex(int idx){
        index = idx;
    }
    
    int getIndex()const{
        return index;
    }
}; // FFMediaConfig

bool FFMediaConfigLessIndex(const FFMediaConfig & m1, const FFMediaConfig & m2);


typedef FFMediaConfig FFAudioConfig;
typedef FFMediaConfig FFVideoConfig;


typedef std::function<int(const AVFrame * )> FFFrameFunc;
typedef std::function<int(int index, const AVFrame * )> FFIndexFrameFunc;
typedef std::function<int(AVPacket * pkt)> FFPacketFunc;


struct FFPacket{
    AVPacket * avpkt = nullptr;
    AVStream * stream = nullptr;
};

class FFReader{
public:
    FFReader(){
        
    }
    
    virtual ~FFReader(){
        close();
    }
    
    virtual int setOptions(const FFImageConfig& cfg){
        char buf[64];
        if(cfg.getWidth() > 0 && cfg.getHeight() > 0){
            sprintf(buf, "%dx%d", cfg.getWidth(), cfg.getHeight());
            av_dict_set(&containerDict_, "video_size", buf, 0);
        }
        
        if(cfg.getFrameRate() > 0){
            av_dict_set_int(&containerDict_, "framerate", cfg.getFrameRate(), 0);
        }
        
        if(cfg.getFormat() > AV_PIX_FMT_NONE){
            av_dict_set(&containerDict_, "pixel_format", av_get_pix_fmt_name(cfg.getFormat()), 0);
        }
        
        return 0;
    }
    
    virtual int open( const std::string& containerFormat
                     , const std::string& url){
        return open(containerFormat, url, std::map<std::string, std::string>());
    }
    
    virtual int open( const std::string& containerFormat
                     , const std::string& url
                     , const std::map<std::string, std::string> cantainerOpt){
        if(formatCtx_){
            // already opened
            return 0;
        }
        
        int ret = -1;
        do{
            formatCtx_ = avformat_alloc_context();
            AVInputFormat *ifmt = NULL;
            if(!containerFormat.empty()){
                ifmt = av_find_input_format(containerFormat.c_str());
                if(!ifmt){
                    //odbge("fail to find format [%s]", containerFormat.c_str());
                    ret = -11;
                    break;
                }
            }
            for(auto o : cantainerOpt){
                av_dict_set(&containerDict_, o.first.c_str(), o.second.c_str(), 0);
            }
            ret = avformat_open_input(&formatCtx_, url.c_str(), ifmt, &containerDict_) ;
            if(ret != 0){
                //odbge("fail to open url=[%s], format=[%s], err=[%d]-[%s]", url.c_str(), containerFormat.c_str(), ret, av_err2str(ret));
                break;
            }
            ret = avformat_find_stream_info(formatCtx_, NULL);
            if(ret < 0){
                //odbge("fail to find stream, [%d]-[%s]", ret, av_err2str(ret));
                break;
            }
            
            avPacket_ = (AVPacket *) av_malloc(sizeof(AVPacket));
            
            ret = 0;
        }while(0);
        
        if(ret){
            this->close();
        }
        return ret;
    }
    
    virtual void close(){
        if(avPacket_){
            av_freep(&avPacket_);
        }
        
        if(formatCtx_){
            avformat_close_input(&formatCtx_);
            //avformat_free_context(formatCtx_);
            formatCtx_ = NULL;
        }
    }
    
    // may block if no packet available
    virtual int read(FFPacket& packet){
        int ret = av_read_frame(formatCtx_, avPacket_);
        if(ret == 0){
            packet.avpkt = avPacket_;
            packet.stream = formatCtx_->streams[avPacket_->stream_index];
        }
        return ret;
    }
    
    virtual int getFirstIndex(AVMediaType typ){
        if(!formatCtx_ || !formatCtx_->streams){
            return -1;
        }
        for(unsigned i = 0; i < formatCtx_->nb_streams; ++i){
            if(typ == formatCtx_->streams[i]->codecpar->codec_type){
                return formatCtx_->streams[i]->index;
            }
        }
        return -1;
    }
    
    virtual int getNumStreams()const {
        if(!formatCtx_){
            return 0;
        }
        return formatCtx_->nb_streams;
    }
    
    virtual const AVStream* getStream(int index)const{
        if(!formatCtx_
           || !formatCtx_->streams
           || index >= formatCtx_->nb_streams){
            return nullptr;
        }
        return formatCtx_->streams[index];
    }
    
protected:
    AVFormatContext        *formatCtx_ = NULL;
    AVPacket               *avPacket_ = NULL;
    AVDictionary*           containerDict_ = NULL;
};




class FFDecoder{
protected:
    AVCodecContext     * codecCtx_ = NULL;
    AVFrame            * avFrame_ = NULL;
    std::string          codecName_;
    FFMediaConfig        srcCfg_;
    const AVStream *     srcStream_ = nullptr;
public:
    virtual ~FFDecoder(){
        close();
    }
    
    virtual const AVCodecContext * getCodecCtx()const{
        return codecCtx_;
    }
    
    virtual void setCfg(const FFMediaConfig& src_cfg){
        srcCfg_ = src_cfg;
    }
    
    virtual void setCfg(const AVStream * stream){
        srcStream_ = stream;
        srcCfg_.assign(stream);
    }
    
    virtual const FFMediaConfig& getCfg()const{
        return srcCfg_;
    }
    
    virtual void setCodecName(const std::string name){
        codecName_ = name;
    }
    
    virtual int open(){
        return open(std::map<std::string, std::string>());
    }
    
    virtual int open(const std::map<std::string, std::string> codecOpt){
        int ret = -1;
        do{
            AVCodec * pCodec = nullptr;
            if(!codecName_.empty()){
                pCodec = avcodec_find_decoder_by_name(codecName_.c_str());
            }
            
            if(!pCodec){
                if(srcCfg_.getCodecId() <= AV_CODEC_ID_NONE){
                    ret = -1;
                    NERROR_FMT_SET(ret, "NOT spec decoder");
                    break;
                }
                
                pCodec = avcodec_find_decoder(srcCfg_.getCodecId());
                if (!pCodec) {
                    ret = -1;
                    NERROR_FMT_SET(ret, "fail to avcodec_find_decoder id [{}]",
                                   (int)srcCfg_.getCodecId());
                    break;
                }
            }
            
            AVDictionary* codecDict = NULL;
            for(auto o : codecOpt){
                //odbgi("stream[%d] codec: dict[%s]=[%s]", index_, o.first.c_str(), o.second.c_str());
                av_dict_set(&codecDict, o.first.c_str(), o.second.c_str(), 0);
            }
            
            this->codecCtx_ = avcodec_alloc_context3(pCodec);
            if(!this->codecCtx_){
                //odbge("fail to alloc codec context, [%d][%s]", pCodec->id, pCodec->name);
                ret = -41;
                break;
            }
            
            if(srcStream_){
                FFUtil::assignDecode(srcStream_, this->codecCtx_);
            }else{
                srcCfg_.get(this->codecCtx_);
            }
            
            if(avcodec_open2(this->codecCtx_, pCodec, &codecDict)<0){
                //odbge("fail to open codec context, [%d][%s]", pCodec->id, pCodec->name);
                ret = -51;
                break;
            }
            
            srcCfg_.assignDecode(this->codecCtx_);
            
            this->avFrame_ = av_frame_alloc();
            ret = 0;
        }while(0);
        
        if(ret){
            this->close();
        }
        return ret;
    }
    
    virtual void close(){
        if(this->avFrame_){
            av_frame_free(&this->avFrame_);
            this->avFrame_ = nullptr;
        }
        if(this->codecCtx_){
            avcodec_close(this->codecCtx_);
            avcodec_free_context(&this->codecCtx_);
            this->codecCtx_ = nullptr;
        }
    }
    
    virtual int decode(const AVPacket* packet, const FFFrameFunc& func){
        int ret = avcodec_send_packet(codecCtx_, packet);
        if(ret){
            NERROR_FMT_SET(ret, "avcodec_send_packet fail with [{}]",
                           av_err2str(ret));
            return ret;
        }
        
        while (ret >= 0) {
            ret = avcodec_receive_frame(codecCtx_, avFrame_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
                return 0;
            }else if (ret != 0) {
                NERROR_FMT_SET(ret, "avcodec_receive_frame fail with[{}]", av_err2str(ret));
                return ret;
            }
            ret = func(avFrame_);
            av_frame_unref(avFrame_);
        }
        return ret;
    }
    
}; // FFDecoder

class FFDecodeReader : public FFReader{
public:
    using Parent = FFReader;
    
private:
    struct Track{
        FFDecoder * decoder = nullptr;
    };
    
public:
    virtual int open( const std::string& containerFormat
                     , const std::string& url
                     , const std::map<std::string, std::string> cantainerOpt)override{
        int ret = Parent::open(containerFormat, url, cantainerOpt);
        if(ret) return ret;
        
        for(unsigned i = 0; i < formatCtx_->nb_streams; ++i){
            AVStream * stream = formatCtx_->streams[i];
            
            tracks_.emplace_back();
            Track& track = tracks_.back();
            track.decoder = new FFDecoder();
            track.decoder->setCfg(stream);
            ret = track.decoder->open();
            if(ret){
                break;
            }
        }
        if(ret){
            close();
        }
        return ret;
        
    }
    
    virtual void close()override{
        Parent::close();
        for(auto& track : tracks_){
            if(track.decoder){
                delete track.decoder;
                track.decoder = nullptr;
            }
        }
        tracks_.clear();
    }
    
    virtual int read(const FFIndexFrameFunc& func){
        
        int ret = 0;
        bool got_frame = false;
        
        while(ret == 0 && !got_frame){
            FFPacket packet;
            ret = Parent::read(packet);
            if(ret){
                return ret;
            }
            int index = packet.avpkt->stream_index;
            Track& track = tracks_[index];
            ret = track.decoder->decode(packet.avpkt, [&index, &got_frame, &func](const AVFrame * frame)->int{
                got_frame = true;
                return func(index, frame);
            });
        }
        
        return ret;
    }
    
    virtual FFMediaConfig getCfg(int index)const{
        if(index >= (int)tracks_.size()){
            return FFMediaConfig();
        }
        return tracks_[index].decoder->getCfg();
    }
    
    virtual const AVCodecContext * getCodecCtx(int index)const{
        if(index >= (int)tracks_.size()){
            return nullptr;
        }
        return tracks_[index].decoder->getCodecCtx();
    }
    
private:
    std::vector<Track> tracks_;
}; // FFDecodeReader


class FFWriter{
private:
    struct Track{
        FFPTSConverter pts_converter;
        AVCodecParameters codecpar;
        AVStream * stream = nullptr;
        bool expect_key = false;
    };
    
public:
    virtual ~FFWriter(){
        close();
    }
    
    int addTrack(const FFMediaConfig& cfg){
        if(cfg.getCodecType() == AVMEDIA_TYPE_AUDIO){
            return addAudioTrack(cfg);
        }else if(cfg.getCodecType() == AVMEDIA_TYPE_VIDEO){
            return addVideoTrack(cfg);
        }else{
            return -1;
        }
    }
    
    int addAudioTrack(const FFAudioConfig& cfg){
        tracks_.emplace_back();
        Track& track = tracks_.back();
        track.pts_converter.setSrcBase(cfg.getTimeBase());
        AVCodecParameters *codecpar = &track.codecpar;
        codecpar->codec_id = cfg.getCodecId();
        
        codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        codecpar->sample_rate = cfg.audio.getSampleRate();
        codecpar->channel_layout = cfg.audio.getChLayout();
        codecpar->channels = av_get_channel_layout_nb_channels(codecpar->channel_layout);
        //stream->codecpar->bit_rate = bit_rate <= 0 ? 24000 : bit_rate;
        codecpar->format = cfg.audio.getFormat(AV_SAMPLE_FMT_S16);
        codecpar->frame_size = cfg.audio.getFrameSize(1024);
        return (int)tracks_.size()-1;
    }
    
    int addVideoTrack(const FFVideoConfig& cfg){
        tracks_.emplace_back();
        Track& track = tracks_.back();
        track.pts_converter.setSrcBase(cfg.getTimeBase());
        AVCodecParameters *codecpar = &track.codecpar;
        codecpar->codec_id = cfg.getCodecId();
        codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        codecpar->width = cfg.video.getWidth();
        codecpar->height = cfg.video.getHeight();
        codecpar->format = cfg.video.getFormat(AV_PIX_FMT_YUV420P);
        codecpar->field_order = AV_FIELD_PROGRESSIVE; // for fix webm file decode error in chrome 56.0 (windows)
        track.expect_key = true;
        /*
         // TODO:
         if(codecid == AV_CODEC_ID_H264){
         if(sps_pps_buffer && sps_pps_length > 0){
         stream->codecpar->extradata = new uint8_t[sps_pps_length];
         memcpy(stream->codecpar->extradata, sps_pps_buffer,sps_pps_length);
         stream->codecpar->extradata_size = sps_pps_length;
         }
         }
         */
        
        return (int)tracks_.size()-1;
    }
    
    bool isOpened()const{
        return fctx_ ? true : false;
    }
    
    int open(const std::string& output_file_name){
        int ret = 0;
        do{
            if(tracks_.empty()){
                ret = -1;
                NERROR_FMT_SET(ret, "no track for write");
                break;
            }
            
            fctx_ = avformat_alloc_context();
            if(fctx_ == NULL) {
                ret = -1;
                NERROR_FMT_SET(ret, "avformat_alloc_context fail");
                break;
            }
            
            fctx_->oformat = av_guess_format(NULL, output_file_name.c_str(), NULL);
            if(fctx_->oformat == NULL) {
                ret = -1;
                NERROR_FMT_SET(ret, "av_guess_format fail");
                break;
            }
            snprintf(fctx_->filename, sizeof(fctx_->filename), "%s", output_file_name.c_str());
            //         if (fctx_->flags & AVFMT_GLOBALHEADER)
            //             vStream_->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            
            ret = 0;
            for(auto& track : tracks_){
                if(track.codecpar.codec_type == AVMEDIA_TYPE_AUDIO){
                    ret = new_audio_stream(fctx_, track);
                }else if(track.codecpar.codec_type == AVMEDIA_TYPE_VIDEO){
                    ret = new_video_stream(fctx_, track);
                }else{
                    ret = -1;
                    NERROR_FMT_SET(ret, "unknown codec type for write, %d", track.codecpar.codec_type);
                    break;
                }
                if(ret) break;
            }
            if(ret) break;
            
            ret = avio_open(&fctx_->pb, fctx_->filename, AVIO_FLAG_WRITE);
            if(ret < 0) {
                NERROR_FMT_SET(ret, "avio_open fail with [%s]", av_err2str(ret));
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
            
            ret = avformat_write_header(fctx_, &opts);
            if(ret < 0) {
                NERROR_FMT_SET(ret, "avformat_write_header fail with [%s]", av_err2str(ret));
                break;
            }
            
            if (opts) {
                AVDictionaryEntry* t = av_dict_get(opts, "", t, 0);
                while (t) {
                    //odbge("invalid opt `%s` found", t->key);
                    t = av_dict_get(opts, "", t, 0);
                }
            }
            write_header_ = true;
            
            for(auto& track : tracks_){
                track.pts_converter.setDstBase(track.stream->time_base);
            }
            
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
        
        if(fctx_ != NULL) {
            avio_close(fctx_->pb);
            fctx_->pb = nullptr;
            avformat_free_context(fctx_);
            fctx_ = nullptr;
        }
    }
    
    int write(int track_index, const AVPacket * src){
        bool is_key_frame = src->flags & AV_PKT_FLAG_KEY;
        
        Track* track;
        int ret = check(track_index, is_key_frame, track);
        if(ret){
            return ret;
        }
        
        AVPacket * packet = av_packet_clone(src);
        packet->stream_index = track->stream->index;
        packet->pts = track->pts_converter.convert(packet->pts);
        packet->dts = track->pts_converter.convert(packet->dts);
        ret = av_interleaved_write_frame(fctx_, packet);
        if(ret) {
            //odbge("write one frame to file %s failed", fctx_->filename);
        }else{
            if(is_key_frame){
                track->expect_key = false;
            }
        }
        av_packet_free(&packet);
        
        return ret;
    }
    
    int write(int track_index,
              int64_t pts, int64_t dts,
              unsigned char * buf, int length,
              bool is_key_frame = false){
        
        Track* track;
        int ret = check(track_index, is_key_frame, track);
        if(ret){
            return ret;
        }
        
        AVPacket packet;
        av_init_packet(&packet);
        packet.stream_index = track->stream->index;
        packet.data = buf;
        packet.size = length;
        packet.pts = track->pts_converter.convert(pts);
        packet.dts = track->pts_converter.convert(dts);
        
        if (is_key_frame){
            packet.flags |= AV_PKT_FLAG_KEY;
        }
        
        ret = av_interleaved_write_frame(fctx_, &packet);
        if(ret < 0) {
            //odbge("write one frame to file %s failed", fctx_->filename);
        }
        return ret;
    }
    
private:
    int check(int track_index, bool is_key_frame, Track*&track){
        if(track_index >= tracks_.size()){
            return -1;
        }
        
        track = &tracks_[track_index];
        if(track->expect_key && !is_key_frame){
            //dbge("discard inter frame without preceeding intra frame");
            return -1;
        }
        return 0;
    }
    
    static
    int new_audio_stream(AVFormatContext * fctx, Track& track){
        int ret = 0;
        AVStream *stream = NULL;
        do{
            const AVCodec *codec = avcodec_find_encoder(track.codecpar.codec_id);
            stream = avformat_new_stream(fctx, codec);
            if(stream == NULL) {
                NERROR_FMT_SET(ret, "avformat_new_stream audio fail");
                ret = -1;
                break;
            }
            
            //avcodec_get_context_defaults3(stream->codec, NULL);
            stream->time_base = track.pts_converter.src();
            stream->codecpar->codec_id = track.codecpar.codec_id;
            stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
            stream->codecpar->sample_rate = track.codecpar.sample_rate;
            stream->codecpar->channels = track.codecpar.channels;
            stream->codecpar->channel_layout = track.codecpar.channel_layout;
            stream->codecpar->frame_size = track.codecpar.frame_size;
            stream->codecpar->format = AV_SAMPLE_FMT_S16;
            //stream->codecpar->bit_rate = track.codecpar.bit_rate <= 0 ? 24000 : bit_rate;
            
//            odbgi("audio-stream: ar=%d, ch=[%d,0x%08llX], bitrate=%lld, frmsz=%d"
//                  , stream->codecpar->sample_rate
//                  , stream->codecpar->channels
//                  , stream->codecpar->channel_layout
//                  , stream->codecpar->bit_rate
//                  , stream->codecpar->frame_size);
            
            //        if (fctx->flags & AVFMT_GLOBALHEADER)
            //            stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            
            track.stream = stream;
            ret = 0;
            
        }while(0);
        
        if(ret){
            // TODO:
        }
        
        return ret;
    }
    
    static
    int new_video_stream(AVFormatContext * fctx, Track& track){
        int ret = 0;
        AVStream *stream = NULL;
        do{
            stream = avformat_new_stream(fctx, 0);
            if(stream == NULL) {
                NERROR_FMT_SET(ret, "avformat_new_stream video fail");
                ret = -1;
                break;
            }
            
            stream->time_base = track.pts_converter.src();
            stream->codecpar->codec_id = track.codecpar.codec_id;
            stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
            stream->codecpar->width = track.codecpar.width;
            stream->codecpar->height = track.codecpar.height;
            stream->codecpar->field_order = track.codecpar.field_order;
            stream->codecpar->format = AV_PIX_FMT_YUV420P; // TODO:
            track.stream = stream;
            ret = 0;
        }while(0);
        
        return ret;
    }
    
private:
    std::vector<Track> tracks_;
    AVFormatContext *fctx_ = nullptr;
    bool write_header_ = false;
    
};


class FFImageAdapter{
public:
    virtual ~FFImageAdapter(){
        close();
    }
    
    int open(const FFImageConfig& dstcfg){
        return open(FFImageConfig(), dstcfg);
    }
    
    int open(const FFImageConfig& srccfg, const FFImageConfig& dstcfg){
        close();
        dstcfg_ = dstcfg;
        int ret = check4Open(srccfg);
        if(ret){
            close();
        }
        return ret;
    }
    
    void close(){
        doClose();
    }
    
    int adapt(const AVFrame * srcframe, const FFFrameFunc& func){
        if(!srcframe){
            return 0;
        }
        
        int ret = 0;
        if(!swsctx_){
            if(!dstcfg_.valid() || dstcfg_.equal(srcframe)){
                return func(srcframe);
            }
        }
        
        if(!srccfg_.equal(srcframe) || !swsctx_){
            FFImageConfig srccfg;
            srccfg.assign(srcframe);
            ret = check4Open(srccfg);
            if(ret){
                return ret;
            }
            if(!swsctx_){
                return func(srcframe);
            }
        }
        
        //av_frame_copy_props(frame_, srcframe);
        frame_->format = dstcfg_.getFormat();
        frame_->width = dstcfg_.getWidth();
        frame_->height = dstcfg_.getHeight();
        
        ret = av_frame_get_buffer(frame_, 32);
        if(ret){
            NERROR_FMT_SET(ret, "fail to av_frame_get_buffer video, err=[{}]", av_err2str(ret));
            return ret;
        }
        
        sws_scale(swsctx_,
                  (const unsigned char* const*)srcframe->data, srcframe->linesize
                  , 0, srcframe->height
                  , frame_->data, frame_->linesize);
        frame_->pts = srcframe->pts;
        frame_->pkt_dts = srcframe->pkt_dts;
        frame_->key_frame = srcframe->key_frame;
        
        ret = func(frame_);
        
        av_frame_unref(frame_);
        return ret;
    }
    
private:
    int check4Open(const FFImageConfig& srccfg){
        const FFImageConfig& dstcfg = dstcfg_;
        if(!srccfg.valid() || !dstcfg.valid() || srccfg.equal(dstcfg)){
            doClose();
            return 0;
        }
        return doOpen(srccfg);
    }
    
    int doOpen(const FFImageConfig& srccfg){
        doClose();
        
        int ret = 0;
        const FFImageConfig& dstcfg = dstcfg_;
        swsctx_ = sws_getContext(srccfg.getWidth(), srccfg.getHeight(), srccfg.getFormat()
                                 , dstcfg.getWidth(), dstcfg.getHeight(), dstcfg.getFormat()
                                 , SWS_BICUBIC, NULL, NULL, NULL);
        
        if(!swsctx_){
            ret = -61;
            NERROR_FMT_SET(ret, "fail to sws_getContext");
            return ret;
        }
        
        frame_ = av_frame_alloc();
        
        srccfg_ = srccfg;
        return ret;
    }
    
    void doClose(){
        if(swsctx_){
            sws_freeContext(swsctx_);
            swsctx_ = NULL;
        }
        
        if(frame_){
            av_frame_free(&frame_);
            frame_ = nullptr;
        }
    }
    
private:
    FFImageConfig srccfg_;
    FFImageConfig dstcfg_;
    SwsContext * swsctx_ = nullptr;
    AVFrame * frame_ = nullptr;
};

#endif /* SDLFramework_hpp */
