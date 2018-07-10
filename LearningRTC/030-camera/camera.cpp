
#include <string>
#include <stdio.h>
extern "C"{
    #include "SDL2/SDL.h"
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libswscale/swscale.h"
    #include "libavdevice/avdevice.h"
    #include "libavutil/imgutils.h"
}

#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)


class FFMpegCameraReader{
protected:
    std::string name_ = "FFCamera";
    std::string deviceName_;
    std::string optFormat_ ;
    std::string optFramerate_ ;
    std::string optPixelFmt_;
    std::string optVideoSize_;
    int width_;
    int height_;
    int framerate_;
    AVPixelFormat outputFormat_ = AV_PIX_FMT_YUV420P; // AV_PIX_FMT_NV12;
    AVFormatContext    *formatCtx_ = NULL;
    AVCodecContext     *codecCtx_ = NULL;
    struct SwsContext  *imgConvertCtx_ = NULL;
    AVPacket   * avPacket_ = NULL;
    AVFrame    * avFrame_ = NULL;
    AVFrame    * avFrameYUV_ = NULL;
    uint8_t    * imageBuffer_ = NULL;
    size_t     imageSize_ = 0;
    AVFrame    * lastFrame_ = NULL;
    
public:
    FFMpegCameraReader(const std::string& deviceName
                       , int width, int height, int framerate
                       , const std::string& optFormat, const std::string& optPixelFmt
                       )
    :deviceName_(deviceName), optFormat_(optFormat), optPixelFmt_(optPixelFmt), framerate_(framerate), width_(width), height_(height) {
        char buf[64];
        sprintf(buf, "%dx%d", width_, height_);
        optVideoSize_ = buf;
        
        sprintf(buf, "%d", framerate_);
        optFramerate_ = buf;
    }
    virtual ~FFMpegCameraReader(){
        this->close();
    }
    int open(){
        if(formatCtx_){
            // already opened
            return 0;
        }
        int ret = -1;
        do{
            formatCtx_ = avformat_alloc_context();
            AVInputFormat *ifmt=av_find_input_format(optFormat_.c_str());
            AVDictionary* options = NULL;
            av_dict_set(&options,"video_size",optVideoSize_.c_str(),0);
            av_dict_set(&options,"framerate", optFramerate_.c_str(),0);
            av_dict_set(&options,"pixel_format", optPixelFmt_.c_str(),0);
            ret = avformat_open_input(&formatCtx_, deviceName_.c_str(), ifmt, &options) ;
            if(ret != 0){
                odbge("fail to open [%s], [%d]-[%s]", deviceName_.c_str(), ret, av_err2str(ret));
                break;
            }
            ret = avformat_find_stream_info(formatCtx_, NULL);
            if(ret < 0){
                odbge("fail to find stream, [%d]-[%s]", ret, av_err2str(ret));
                break;
            }
            
            AVCodecID codec_id = AV_CODEC_ID_NONE;
            for(int i=0; i< formatCtx_->nb_streams; i++){
                AVMediaType mediaType = formatCtx_->streams[i]->codecpar->codec_type;
                if(mediaType == AVMEDIA_TYPE_VIDEO){
                    codec_id = formatCtx_->streams[i]->codecpar->codec_id;
                    break;
                }
            }
            if(codec_id == AV_CODEC_ID_NONE){
                odbge("fail to find a video stream");
                ret = -21;
                break;
            }
            AVCodec * pCodec = avcodec_find_decoder(codec_id);
            if(!pCodec){
                odbge("fail to find codec [%d]", codec_id);
                ret = -31;
                break;
            }
            codecCtx_ = avcodec_alloc_context3(pCodec);
            if(!codecCtx_){
                odbge("fail to alloc codec context");
                ret = -41;
                break;
            }
            AVDictionary* codecOpt = NULL;
            av_dict_set(&codecOpt,"video_size", optVideoSize_.c_str(), 0);
            av_dict_set(&codecOpt,"pixel_format", optPixelFmt_.c_str(),0);
            if(avcodec_open2(codecCtx_, pCodec, &codecOpt)<0){
                odbge("fail to open codec context");
                ret = -51;
                break;
            }
            
            avPacket_ = (AVPacket *) av_malloc(sizeof(AVPacket));
            avFrame_ = av_frame_alloc();
            
            if(codecCtx_->pix_fmt != outputFormat_){
                imgConvertCtx_ = sws_getContext(codecCtx_->width, codecCtx_->height, codecCtx_->pix_fmt
                                                , codecCtx_->width, codecCtx_->height, outputFormat_
                                                , SWS_BICUBIC, NULL, NULL, NULL);
                if(!imgConvertCtx_){
                    odbge("fail to create image converter");
                    ret = -61;
                    break;
                }
                
                // alloc yuv buffer
                imageSize_ = av_image_get_buffer_size(outputFormat_, codecCtx_->width, codecCtx_->height, 1);
                imageBuffer_ = (uint8_t *) av_malloc(imageSize_);
                
                // alloc yuv frame with yuv buffer
                avFrameYUV_ = av_frame_alloc();
                avFrameYUV_->width = codecCtx_->width;
                avFrameYUV_->height = codecCtx_->height;
                avFrameYUV_->format = outputFormat_;
                av_image_fill_arrays(avFrameYUV_->data, avFrameYUV_->linesize, imageBuffer_, outputFormat_, codecCtx_->width, codecCtx_->height, 1);
            }
            
            ret = 0;
        }while(0);
        
        if(ret){
            this->close();
        }
        return ret;
    }
    
    void close(){
        if(imgConvertCtx_){
            sws_freeContext(imgConvertCtx_);
            imgConvertCtx_ = NULL;
        }
        av_freep(&avPacket_);
        av_frame_free(&avFrame_);
        av_frame_free(&avFrameYUV_);
        av_freep(&imageBuffer_);
        avcodec_free_context(&codecCtx_);
        lastFrame_ = NULL;
    }
    
    // may block if no frame available
    int readFrame(){
        int ret = -1;
        do{
            // try to read remain frame in codec context
            ret = avcodec_receive_frame(codecCtx_, avFrame_);
            if(ret == 0){
                break;
            }
            ret = av_read_frame(formatCtx_, avPacket_);
            if(ret < 0){
                odbge("fail to read frame, [%d]-[%s]", ret, av_err2str(ret));
                break;
            }
            avcodec_send_packet(codecCtx_, avPacket_);
            ret = avcodec_receive_frame(codecCtx_, avFrame_);
        }while(0);

        if(ret == 0){
            if(imgConvertCtx_){
                sws_scale(imgConvertCtx_, (const unsigned char* const*)avFrame_->data, avFrame_->linesize
                          , 0, avFrame_->height
                          , avFrameYUV_->data, avFrameYUV_->linesize);
                avFrameYUV_->width = avFrame_->width;
                avFrameYUV_->height = avFrame_->height;
                avFrameYUV_->pkt_size = (int)imageSize_;
                lastFrame_ = avFrameYUV_;
            }else{
                lastFrame_ = avFrame_;
            }
        }
        //av_frame_unref(avFrame_); // TODO: check calling av_frame_unref?
        return ret;
    }
    
    int frameWidth(){
        if(!lastFrame_) return 0;
        return lastFrame_->width;
    }
    
    int frameHeight(){
        if(!lastFrame_) return 0;
        return lastFrame_->height;
    }
    
    int frameSize(){
        if(!lastFrame_) return 0;
        return lastFrame_->pkt_size;
    }
    
    uint8_t ** frameData(){
        if(!lastFrame_) return NULL;
        return lastFrame_->data;
    }
    
    int * frameLineSize(){
        if(!lastFrame_) return NULL;
        return lastFrame_->linesize;
    }
};

static
std::string getFilePath(const std::string& relativePath){
    static std::string path;
    static char div = '\0';
    if(!div){
        const char * srcpath = __FILE__;
        div = '/';
        const char * p = strrchr(srcpath, div);
        if(!p){
            div = '\\';
            p = strrchr(__FILE__, div);
        }
        if(p){
            path = srcpath;
            path = path.substr(0, p-srcpath) + div;
        }
    }
    return path + relativePath;
}

int main(int argc, char* argv[]){
    std::string name_ = "main";
    av_register_all();
    avformat_network_init();
    avdevice_register_all();
    av_log_set_level(AV_LOG_QUIET);
    
    // for Mac: ffmpeg -list_devices true -f avfoundation -i dummy
    const std::string& deviceName = "FaceTime HD Camera"; // or "2";
    const std::string& optFormat = "avfoundation";
    const std::string& optPixelFmt = "yuyv422";
    int width = 640;
    int height = 480;
    int framerate = 30;
    
    char fname[128];
    sprintf(fname, "../data/camera_out_yuv420p_%dx%d.yuv", width, height);
    std::string outFilePath = getFilePath(fname);

    FFMpegCameraReader * reader = NULL;
    FILE * outfp = NULL;
    int ret = -1;
    do{
        reader = new FFMpegCameraReader(deviceName, width, height, framerate, optFormat, optPixelFmt);
        ret = reader->open();
        if(ret){
            odbge("fail to open camera, ret=%d", ret);
            break;
        }
        
        outfp = fopen(outFilePath.c_str(), "wb");
        if(!outfp){
            odbge("fail to write open [%s]", outFilePath.c_str());
            ret = -1;
            break;
        }
        
        int nframe = 0;
        int maxframes = 120;
        
        uint32_t startTime = SDL_GetTicks();
        while(nframe < maxframes){
            int ret = reader->readFrame();
            if(ret){
                break;
            }
            ++nframe;
            ret = (int)fwrite(reader->frameData()[0], sizeof(char), reader->frameSize(), outfp);
            if(ret != reader->frameSize()){
                odbge("fail to write file, ret=[%d]", ret);
                break;
            }
            odbgi("write frame %d/%d", nframe, maxframes);
        }
        uint32_t elapsed = SDL_GetTicks() - startTime;
        odbgi("write frames %d in %u ms, fps=%d", nframe, elapsed, 1000*nframe/elapsed);
        odbgi("output file [%s]", outFilePath.c_str());
        ret = 0;
    }while(0);
    if(outfp){
        fclose(outfp);
        outfp = NULL;
    }
    if(reader){
        delete reader;
        reader = NULL;
    }
    return ret;
}
