
#include <string>
#include <map>
#include <vector>
#include <list>
#include <stdio.h>
extern "C"{
    #include "SDL2/SDL.h"
    #include "SDL2/SDL_ttf.h"
//    #include "libavcodec/avcodec.h"
//    #include "libavformat/avformat.h"
//    #include "libswscale/swscale.h"
//    #include "libswresample/swresample.h"
//    #include "libavdevice/avdevice.h"
//    #include "libavutil/imgutils.h"
    
    #include "vpx/vpx_encoder.h"
    #include "vpx/vp8cx.h"
    #include "vpx/vpx_decoder.h"
    #include "vpx/vp8dx.h"
}

#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)

class SDLWindow{
public:
    virtual ~SDLWindow(){}
    virtual void refresh() = 0;
    virtual SDL_Renderer* getRenderer() = 0;
    virtual int getWidth() = 0;
    virtual int getHeight() = 0;
};

class SDLView{
protected:
    SDLWindow * win_ = NULL;
    int x_ = 0;
    int y_ = 0;
    SDL_Texture* sdlTexture_ = NULL;

public:
    SDLView(){
    }
    virtual ~SDLView(){
        if(sdlTexture_){
            SDL_DestroyTexture(sdlTexture_);
            sdlTexture_ = NULL;
        }
    }
    virtual void getPosition(int &x, int &y){
        x = x_;
        y = y_;
    }
    virtual void setPosition(int x, int y){
        x_ = x;
        y_ = y;
    }
    virtual void setWindow(SDLWindow * win){
        win_ = win;
    }
    virtual void onDraw() = 0;

};

class SDLWindowImpl : public SDLWindow{
protected:
    int width_;
    int height_;
    std::string name_;
    SDL_Window * window_ = NULL;
    SDL_Renderer* sdlRenderer_ = NULL;
    std::list<SDLView *> subViews;
public:
    SDLWindowImpl(const std::string& name, int w, int h):name_(name), width_(w), height_(h){
    }
    
    virtual ~SDLWindowImpl(){
        this->close();
    }
    
    int open(int x = -1, int y = -1){
        int ret = -1;
        do{
            if(x < 0)  x = SDL_WINDOWPOS_CENTERED;
            if(y < 0)  y = SDL_WINDOWPOS_CENTERED;
            window_ = SDL_CreateWindow(name_.c_str(), x, y, width_, height_,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
            if(!window_) {
                odbge("SDL: could not create window - exiting:%s\n",SDL_GetError());
                ret = -11;
                break;
            }
            sdlRenderer_ = SDL_CreateRenderer(window_, -1, 0);
            SDL_RaiseWindow(window_);
            ret = 0;
        }while(0);
        if(ret){
            this->close();
        }
        return ret;
    }
    
    void close(){
        for(auto v : subViews){
            delete v;
        }
        subViews.clear();
        
        if(sdlRenderer_){
            SDL_DestroyRenderer(sdlRenderer_);
            sdlRenderer_ = NULL;
        }
        if(window_){
            SDL_DestroyWindow(window_);
            window_ = NULL;
        }
    }
    
    void setPosition(int x, int y){
        
    }
    
    void addView(SDLView * view){
        for(auto v : subViews){
            if(v == view ) return;
        }
        subViews.push_back(view);
        view->setWindow(this);
    }
    
    virtual void refresh() override{
        SDL_RenderClear( sdlRenderer_ );
        for(auto v : subViews){
            v->onDraw();
        }
        SDL_RenderPresent( sdlRenderer_ );
    }
    
    virtual SDL_Renderer* getRenderer() override{
        return sdlRenderer_;
    }
    
    virtual int getWidth()override{
        return width_;
    }
    virtual int getHeight()override{
        return height_;
    }
};


class SDLTextView : public SDLView{
protected:
    TTF_Font * font_ = NULL;
    SDL_Surface * surface_ = NULL;
    std::string text_;
    SDL_Rect rect_;
    
    void freeImage(){
        if(sdlTexture_){
            SDL_DestroyTexture(sdlTexture_);
            sdlTexture_ = NULL;
        }
        
        if(surface_){
            SDL_FreeSurface(surface_);
            surface_ = NULL;
        }
    }
public:
    SDLTextView(){
        font_ = TTF_OpenFont("/Library/Fonts/Arial.ttf", 20);
    }
    
    virtual ~SDLTextView(){
        this->freeImage();
        if(font_){
            TTF_CloseFont(font_);
            font_ = NULL;
        }
    }
    
    int draw(const std::string& text){
        this->freeImage();
        text_ = text;
        if(win_){
            win_->refresh();
        }
        return 0;
    }
    
    virtual void onDraw()override{
        if(!sdlTexture_ && win_){
            SDL_Color color = { 255, 0, 0 };
            surface_ = TTF_RenderText_Solid(font_, text_.c_str(), color);
            sdlTexture_ = SDL_CreateTextureFromSurface(win_->getRenderer(), surface_);
            rect_.x = x_;
            rect_.y = y_;
            SDL_QueryTexture(sdlTexture_, NULL, NULL, &rect_.w, &rect_.h);
            //TTF_SizeUTF8(font_, text_.c_str(), &rect_.w, &rect_.h);
        }
        if(sdlTexture_){
            SDL_RenderCopy( win_->getRenderer(), sdlTexture_, NULL, &rect_);
        }
    }
};

class SDLVideoView : public SDLView{
public:
    
    void drawYUV(int w, int h
                 , const Uint8 *Yplane, int Ypitch
                 , const Uint8 *Uplane, int Upitch
                 , const Uint8 *Vplane, int Vpitch){
        if(sdlTexture_){
            int tw = 0;
            int th = 0;
            SDL_QueryTexture(sdlTexture_, NULL, NULL, &tw, &th);
            if(tw != w || th != h){
                SDL_DestroyTexture(sdlTexture_);
                sdlTexture_ = NULL;
            }
        }
        
        if(!sdlTexture_ && win_){
            sdlTexture_ = SDL_CreateTexture(win_->getRenderer(), SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, w, h);
        }
        
        if(sdlTexture_){
            SDL_UpdateYUVTexture(sdlTexture_, NULL, Yplane, Ypitch, Uplane, Upitch, Vplane, Vpitch);
        }
        
        if(win_){
            win_->refresh();
        }
    }
    
    virtual void onDraw()override{
        if(sdlTexture_ && win_){
            SDL_RenderCopy( win_->getRenderer(), sdlTexture_, NULL, NULL);
        }
    }
};


class VP8Encoder{
    
protected:
    vpx_codec_ctx_t encctx_;
    int width_ = 0;
    int height_ = 0;
    int framerate_ = 0;
    int bitrateKbps_ = 0;
    vpx_codec_pts_t pts_ = 0;
    uint8_t * encodedBuf_ = NULL;
    size_t encodedBufSize_ = 0;
    size_t frameLength_ = 0;
    vpx_codec_frame_flags_t frameFlags_ = 0;
    
    void freeEncodeBuf(){
        if(encodedBuf_){
            delete []encodedBuf_;
            encodedBuf_ = NULL;
        }
        frameLength_ = 0;
        encodedBufSize_ = 0;
    }
    
    void appendEncodedData(void *data, size_t length){
        if(length == 0) return;
        if((frameLength_ + length) > encodedBufSize_){
            size_t frmlen = frameLength_;
            size_t bufsz = frmlen + length;
            size_t blocksz = 64*1024;
            bufsz = ((bufsz + blocksz -1)/blocksz) * blocksz; // align to blocksz
            uint8_t * buf = new uint8_t[bufsz];
            if(frmlen > 0){
                memcpy(buf, encodedBuf_, frmlen);
            }
            this->freeEncodeBuf();
            encodedBuf_ = buf;
            encodedBufSize_ = bufsz;
            frameLength_ = frmlen;
        }
        memcpy(encodedBuf_+frameLength_, data, length);
        frameLength_ += length;
    }
    
public:
    VP8Encoder(){
        memset(&encctx_, 0, sizeof(encctx_));
        encctx_.name = NULL;
    }
    
    virtual ~VP8Encoder(){
        this->close();
    }
    
    int open(int width, int height, int framerate, int bitrateKbps){
        vpx_codec_enc_cfg_t enccfg;
        vpx_codec_flags_t encflags = 0;
        vpx_codec_err_t vpxret = VPX_CODEC_OK;
        int ret = 0;
        
        do{
            this->close();
            
            width_ = width;
            height_ = height;
            bitrateKbps_ = bitrateKbps;
            framerate_ = framerate;
            
            vpxret = vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &enccfg, 0);
            enccfg.g_w = width_;
            enccfg.g_h = height_;
            enccfg.rc_target_bitrate = bitrateKbps_; // in unit of kbps
            //        enccfg.g_timebase.num = info.time_base.numerator;
            //        enccfg.g_timebase.den = info.time_base.denominator;
            //        enccfg.g_error_resilient = (vpx_codec_er_flags_t)strtoul(argv[7], NULL, 0);
            encflags |= VPX_CODEC_USE_OUTPUT_PARTITION;
            vpxret = vpx_codec_enc_init_ver(&encctx_,
                                            vpx_codec_vp8_cx(),
                                            &enccfg,
                                            encflags, VPX_ENCODER_ABI_VERSION);
            if(vpxret != VPX_CODEC_OK){
                ret = -1;
                encctx_.name = NULL;
                break;
            }
            
            pts_  =0;
            ret = 0;
        }while(0);
        
        if(ret){
            close();
        }
        
        return ret;
    }
    
    void close(){
        if(encctx_.name){
            vpx_codec_destroy(&encctx_);
            encctx_.name = NULL;
        }
        this->freeEncodeBuf();
    }
    
    int encode(vpx_image_t * vpximg, bool forceKeyframe = false){
        vpx_codec_err_t vpxret = VPX_CODEC_OK;
        int ret = -1;
        do{
            frameLength_ = 0;
            frameFlags_ = 0;
            
            uint32_t duration = 90000 / framerate_;
            vpx_enc_frame_flags_t frameflags = 0;
            if (forceKeyframe){
                frameflags |= VPX_EFLAG_FORCE_KF;
            }
            
            vpxret = vpx_codec_encode(&encctx_, vpximg, pts_, duration, frameflags, VPX_DL_REALTIME);
            if (vpxret != VPX_CODEC_OK) {
                odbge("vpx_codec_encode fail with %d", vpxret);
                ret = -1;
                break;
            }
            
            vpx_codec_iter_t iter = NULL;
            const vpx_codec_cx_pkt_t *pkt = NULL;
            int npkts = 0;
            while ((pkt = vpx_codec_get_cx_data(&encctx_, &iter)) != NULL) {
                if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
                    const int keyframe = (pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0;
                    odbgi("  pkt[%d]: pts=%lld, data=%p, sz=%zu, part=%d, k=%d"
                          , npkts
                          , pkt->data.frame.pts
                          , pkt->data.frame.buf
                          , pkt->data.frame.sz
                          , pkt->data.frame.partition_id
                          , keyframe);
                    frameFlags_ = pkt->data.frame.flags;
                    this->appendEncodedData(pkt->data.frame.buf, pkt->data.frame.sz);

                    ++npkts;
                }else{
                    odbgi("  non-frame-pkt, kind=%d", pkt->kind);
                }
            }
            
            ret = 0;
        }while(0);
        return ret;
    }
    
    uint8_t * getEncodedFrame(){
        return this->encodedBuf_;
    }
    
    size_t getEncodedLength(){
        return frameLength_;
    }
    
    vpx_codec_frame_flags_t getFrameFlags(){
        return frameFlags_;
    }
};

class VP8Decoder{
    
protected:
    vpx_codec_ctx_t decctx_;
    vpx_codec_iter_t imageIter_ = NULL;
public:
    VP8Decoder(){
        memset(&decctx_, 0, sizeof(decctx_));
        decctx_.name = NULL;
    }
    
    virtual ~VP8Decoder(){
        this->close();
    }
    
    int open(){
        vpx_codec_err_t vpxret = VPX_CODEC_OK;
        int ret = 0;
        do{
            this->close();
            
            vpx_codec_dec_cfg_t deccfg;
            deccfg.threads = 1;
            deccfg.h = deccfg.w = 0;  // set after decode
            vpx_codec_flags_t decflags = 0;
            //        decflags = VPX_CODEC_USE_POSTPROC;
            //        decflags |= VPX_CODEC_USE_INPUT_PARTITION;
            //        decflags |= VPX_CODEC_USE_INPUT_FRAGMENTS;
            
            vpxret = vpx_codec_dec_init_ver(&decctx_, vpx_codec_vp8_dx(), &deccfg, decflags, VPX_DECODER_ABI_VERSION);
            if(vpxret != VPX_CODEC_OK){
                ret = -1;
                decctx_.name = NULL;
                break;
            }
            ret = 0;
        }while(0);
        
        if(ret){
            close();
        }
        
        return ret;
    }
    
    void close(){
        if(decctx_.name){
            vpx_codec_destroy(&decctx_);
            decctx_.name = NULL;
        }
        imageIter_ = NULL;
    }
    
    
    int decode(const uint8_t * frameData, size_t frameLength
               , int * pRefUpdate = NULL
               , int * pCorrupted = NULL){
        imageIter_ = NULL;
        vpx_codec_err_t vpxret = vpx_codec_decode(&decctx_, frameData, (unsigned int)frameLength, NULL, 0);
        if (vpxret != VPX_CODEC_OK) {
            odbge("vpx_codec_decode fail with %d", vpxret);
            return -1;
        }
        
        int refupdates = 0;
        vpxret = vpx_codec_control(&decctx_, VP8D_GET_LAST_REF_UPDATES, &refupdates);
        if (vpxret != VPX_CODEC_OK) {
            odbge("vpx_codec_control VP8D_GET_LAST_REF_UPDATES fail with %d", vpxret);
        }
        
        int corrupted = 0;
        vpxret = vpx_codec_control(&decctx_, VP8D_GET_FRAME_CORRUPTED, &corrupted);
        if (vpxret != VPX_CODEC_OK) {
            odbge("vpx_codec_control VP8D_GET_FRAME_CORRUPTED fail with %d", vpxret);
        }
        odbgi("decode: ref=0x%02x, corrupted=%d", refupdates, corrupted);
        if (((refupdates & VP8_GOLD_FRAME) ||
             (refupdates & VP8_ALTR_FRAME)) &&
            !corrupted) {
            
        }
        
        if(pRefUpdate){
            *pRefUpdate = refupdates;
        }
        if(pCorrupted){
            *pCorrupted = corrupted;
        }
        
        return 0;
    }
    
    vpx_image_t * pullImage(){
        vpx_image_t * decimg = vpx_codec_get_frame(&decctx_, &imageIter_);
        return decimg;
    }
};



static
int vpx_img_plane_width(const vpx_image_t *img, int plane) {
    if (plane > 0 && img->x_chroma_shift > 0)
        return (img->d_w + 1) >> img->x_chroma_shift;
    else
        return img->d_w;
}

static
int vpx_img_plane_height(const vpx_image_t *img, int plane) {
    if (plane > 0 && img->y_chroma_shift > 0)
        return (img->d_h + 1) >> img->y_chroma_shift;
    else
        return img->d_h;
}

static
int vpx_img_read(vpx_image_t *img, FILE *file) {
    int factor = ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    for (int plane = 0; plane < 3; ++plane) {
        unsigned char *buf = img->planes[plane];
        const int stride = img->stride[plane];
        const int w = vpx_img_plane_width(img, plane) * factor;
        const int h = vpx_img_plane_height(img, plane);
        int y;
        
        for (y = 0; y < h; ++y) {
            if (fread(buf, 1, w, file) != (size_t)w){
                return -1;
            }
            buf += stride;
        }
    }
    
    return 0;
}

static
void dump_vp8_frame(uint8_t * frameData, size_t frameSize){
    uint8_t firstByte = frameData[0];
    int frmtype = (firstByte >> 0 & 0x1);
    int ver = firstByte >> 1 & 0x7;
    int disp = (firstByte >> 4 & 0x1) ;
    int part1Len = (((firstByte >> 5) & 0x7) << 0) | (frameData[1] << 3) | (frameData[2] << 11);
    odbgi("  frame: type=%d, ver=%d, disp=%d, part1=%d", frmtype, ver, disp, part1Len);
    if(frmtype == 0){
        // startcode=[9d 01 2a]
        // Horizontal 16 bits      :     (2 bits Horizontal Scale << 14) | Width (14 bits)
        // Vertical   16 bits      :     (2 bits Vertical Scale << 14) | Height (14 bits)
        uint8_t * ptr = frameData + 3;
        uint16_t wordH = ptr[4] << 8 | ptr[3];
        uint16_t wordV = ptr[6] << 8 | ptr[5];
        int scaleH = (wordH >> 14) & 0x3;
        int width = wordH & 0x3FFF;
        int scaleV = (wordV >> 14) & 0x3;
        int height = wordV & 0x3FFF;
        
        uint8_t * part1Data = (frameData+10);
        int color = part1Data[0] & 0x1;
        int pixel = (part1Data[0]>>1) & 0x1;
        
        odbgi("         startcode=[%02x %02x %02x], horiz=[%d, %d], vert=[%d, %d], color=%d, pixel=%d"
              , ptr[0], ptr[1], ptr[2]
              , scaleH, width, scaleV, height
              , color, pixel);
    }
    
}

class AppDemo{
    vpx_image_t * vpximg_ = NULL;
    SDLWindowImpl * window1_ = NULL;
    SDLWindowImpl * window2_ = NULL;
    SDLVideoView * videoView1_ = NULL;
    SDLVideoView * videoView2_ = NULL;
    SDLTextView * frameTxtView1_ = NULL;
    SDLTextView * frameTxtView2_ = NULL;
    
    VP8Encoder * encoder_ = NULL;
    VP8Decoder * decoder_ = NULL;
    FILE * fp_ = NULL;
    int numFrames_ = 0;
    size_t numEncodeBytes_ = 0;
    int framerate_ = 0;
    int keyInterval_ = 0;
    uint32_t startTime_ = 0;
    bool drop_ = false;
public:
    void close(){
        uint32_t elapsed = SDL_GetTicks() - startTime_;
        if(numFrames_ > 0 && elapsed > 0){
            odbgi("final: elapsed=%d ms, frames=%d, encBytes=%zu, fps=%d, bps=%zu"
                  , elapsed, numFrames_, numEncodeBytes_, 1000*numFrames_/elapsed, 8*numEncodeBytes_*framerate_/numFrames_);
        }
        if(window1_){
            delete window1_;
            window1_ = NULL;
        }
        if(window2_){
            delete window2_;
            window2_ = NULL;
        }
        if(decoder_){
            delete decoder_;
            decoder_ = NULL;
        }
        if(encoder_){
            delete encoder_;
            encoder_ = NULL;
        }
        if(vpximg_){
            vpx_img_free(vpximg_);
            vpximg_ = NULL;
        }
        if(fp_){
            fclose(fp_);
            fp_ = NULL;
        }
        numFrames_ = 0;
        numEncodeBytes_ = 0;
        drop_ = false;
    }
    
    int open(const char * yuvfile, int width, int height, int framerate, int keyInterval = 0){
        int ret = -1;
        do{
            this->close();
            
            framerate_ = framerate;
            keyInterval_ = keyInterval;
            
            encoder_ = new VP8Encoder();
            decoder_ = new VP8Decoder();
            window1_ = new SDLWindowImpl("raw", width, height);
            window2_ = new SDLWindowImpl("decode", width, height);
            videoView1_ = new SDLVideoView();
            videoView2_ = new SDLVideoView();
            frameTxtView1_ = new SDLTextView();
            frameTxtView2_ = new SDLTextView();
            
            int bitrateKbps = 500;
            ret = encoder_->open(width, height, framerate, bitrateKbps);
            if(ret) break;
            
            ret = decoder_->open();
            if(ret) break;
            
            int x1 = -1;
            int x2 = -1;
            int y = -1;
            SDL_DisplayMode displayMode;
            ret = SDL_GetCurrentDisplayMode(0, &displayMode);
            if(ret == 0){
                int cx = displayMode.w/2;
                int cy = displayMode.h/2;
                x1 = cx > width ? cx-width : 0;
                x2 = cx;
                y = cy > height ? cy-height/2 : 0;
            }

            window1_->open(x1, y);
            if(ret) break;

            window2_->open(x2, y);
            if(ret) break;
            
            window1_->addView(videoView1_);
            window1_->addView(frameTxtView1_);
            window2_->addView(videoView2_);
            window2_->addView(frameTxtView2_);
            
            frameTxtView1_->draw(" press 'i' -> Key Frame, press 'p' -> P Frame, 'ctrl+' -> drop");
            frameTxtView2_->draw(" press 'q' -> Quit");
            
            fp_ = fopen(yuvfile, "rb");
            if (!fp_) {
                odbge("fail to open yuv file [%s]", yuvfile);
                ret = -1;
                break;
            }
            
            vpximg_ = vpx_img_alloc(NULL, VPX_IMG_FMT_I420, width, height, 1);
            if(!vpximg_){
                ret = -1;
                break;
            }
            startTime_ = SDL_GetTicks();
            ret = 0;
        }while(0);
        if(ret){
            this->close();
        }
        return ret;
    }
    
    int makeTextCommon(char * txt){
        bool isKey = (encoder_->getFrameFlags()&VPX_FRAME_IS_KEY)!=0;
        return sprintf(txt, " Frame %d, type=%c", numFrames_, isKey?'I':'P');
    }
    
    int processOneFrame(bool enckey, bool drop){
        int ret = 0;
        do{
            ret = vpx_img_read(vpximg_, fp_);
            if(ret < 0){
                break;
            }
            videoView1_->drawYUV(vpximg_->d_w, vpximg_->d_h
                                 , vpximg_->planes[0], vpximg_->stride[0]
                                 , vpximg_->planes[1], vpximg_->stride[1]
                                 , vpximg_->planes[2], vpximg_->stride[2]);

            
            odbgi("--- enc frame %d", numFrames_);
            ret = encoder_->encode(vpximg_, enckey);
            if(ret){
                break;
            }
            ++numFrames_;
            char txt[128];
            int txtlen = makeTextCommon(txt+0);
            if(drop){
                txtlen += sprintf(txt+txtlen, ", drop=%d", drop);
            }
            frameTxtView1_->draw(txt);

            numEncodeBytes_ += encoder_->getEncodedLength();
            if(encoder_->getEncodedLength() > 0){
                dump_vp8_frame(encoder_->getEncodedFrame(), encoder_->getEncodedLength());
                int refupdate;
                int corrupted;
                if(drop){
                    ret = decoder_->decode(NULL, 0, &refupdate, &corrupted);
                }else{
                    ret = decoder_->decode(encoder_->getEncodedFrame(), encoder_->getEncodedLength(), &refupdate, &corrupted);
                }
                if (ret == 0) {
                    vpx_image_t * decimg = NULL;
                    while ((decimg = decoder_->pullImage()) != NULL) {
                        videoView2_->drawYUV(decimg->d_w, decimg->d_h
                                             , decimg->planes[0], decimg->stride[0]
                                             , decimg->planes[1], decimg->stride[1]
                                             , decimg->planes[2], decimg->stride[2]);
                        int txtlen = this->makeTextCommon(txt+0);
                        if(corrupted){
                            txtlen += sprintf(txt+txtlen, ", corrupted" );
                        }
                        if(refupdate & VP8_GOLD_FRAME){
                            txtlen += sprintf(txt+txtlen, ", gold" );
                        }
                        if(refupdate & VP8_ALTR_FRAME){
                            txtlen += sprintf(txt+txtlen, ", altr" );
                        }
                        frameTxtView2_->draw(txt);
                    }
                }
            }
            ret = 0;
        }while(0);
        return ret;
    }
    
    void refresh(){
        if(window1_){
            window1_->refresh();
        }
        if(window2_){
            window2_->refresh();
        }
    }
};

static
int encode_file(const char * yuvfile, int width, int height, int framerate, int keyInterval = 0){
    int ret = 0;
    AppDemo * app = new AppDemo();
    do{
        ret = app->open(yuvfile, width, height, framerate, keyInterval);
        if(ret) break;
        
        SDL_Event event;
        bool quitLoop = false;
        while(!quitLoop){
            SDL_WaitEvent(&event);
            if(event.type==SDL_QUIT){
                odbgi("got QUIT event %d", event.type);
                quitLoop = true;
                break;
            }else if(event.type == SDL_WINDOWEVENT){
                if(event.window.event == SDL_WINDOWEVENT_CLOSE){
                    odbgi("Window %d closed", event.window.windowID);
                    quitLoop = true;
                    break;
                }else if(event.window.event == SDL_WINDOWEVENT_RESIZED){
                    odbgi("Window %d resized to %dx%d"
                          , event.window.windowID
                          , event.window.data1, event.window.data2);
                    app->refresh();
                }
            }else if(event.type == SDL_KEYDOWN){
                odbgi("got keydown event %d, win=%d, key=%d, scan=%d, mod=%d", event.type, event.key.windowID
                      , event.key.keysym.sym, event.key.keysym.scancode, event.key.keysym.mod);
                if(event.key.keysym.sym == SDLK_p){
                    bool drop = (event.key.keysym.mod & KMOD_CTRL)!=0;
                    ret = app->processOneFrame(false, drop);
                }else if(event.key.keysym.sym == SDLK_i){
                    bool drop = (event.key.keysym.mod & KMOD_CTRL)!=0;
                    ret = app->processOneFrame(true, drop);
                }else if(event.key.keysym.sym == SDLK_q){
                    quitLoop = true;
                }
                if(ret){
                    quitLoop = true;
                }
            }
        }// while(!quitLoop)
        ret = 0;
    }while(0);
    
    if(app){
        delete app;
        app = NULL;
    }
    return ret;
}


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

extern "C"{
    int lab_vp8_main(int argc, char* argv[]);
}

int lab_vp8_main(int argc, char* argv[]){
    int ret = SDL_Init(SDL_INIT_VIDEO) ;
    if(ret){
        odbge( "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
    ret = TTF_Init();
    
    std::string filepath = getFilePath("../../LearningRTC/data/test_yuv420p_640x360.yuv");
    ret = encode_file(filepath.c_str(), 640, 360, 25, 25);
    
    TTF_Quit();
    SDL_Quit();
    return ret;
}
