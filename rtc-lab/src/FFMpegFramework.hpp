

#ifndef FFMpegFramework_hpp
#define FFMpegFramework_hpp

#include <stdio.h>
#include <string>
#include <vector>
#include <map>

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
    
};
#endif /* SDLFramework_hpp */
