
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
    vpx_codec_enc_cfg_t enccfg_;
    int width_ = 0;
    int height_ = 0;
    int framerate_ = 0;
    int bitrateKbps_ = 0;
    uint8_t * encodedBuf_ = NULL;
    size_t encodedBufSize_ = 0;
    size_t frameLength_ = 0;
    vpx_codec_frame_flags_t frameFlags_ = 0;
    int numFrames_ = 0;
    int flagsOfTLayer_[16];
    int flagsOfTLayerLength_ = 0;
    int flagIndex_ = 0;
    
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
    
    int open(int width, int height, int framerate, int bitrateKbps, int numTLayers = 1){
        
        vpx_codec_flags_t encflags = 0;
        vpx_codec_err_t vpxret = VPX_CODEC_OK;
        int ret = 0;
        
        do{
            this->close();
            if(numTLayers < 1){
                numTLayers = 1;
            }
            width_ = width;
            height_ = height;
            bitrateKbps_ = bitrateKbps;
            framerate_ = framerate;
            
            vpxret = vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &enccfg_, 0);
            enccfg_.g_w = width_;
            enccfg_.g_h = height_;
            enccfg_.rc_target_bitrate = bitrateKbps_; // in unit of kbps
//            enccfg_.kf_mode = VPX_KF_AUTO;
            enccfg_.g_error_resilient = 1;
            
            if(numTLayers > 1){
                enccfg_.ts_number_layers = numTLayers;
                enccfg_.ts_periodicity = numTLayers;
                int br = bitrateKbps / numTLayers;
                for(int i = 0; i < numTLayers; ++i){
                    enccfg_.ts_target_bitrate[i] = (i+1)*br;
                    enccfg_.ts_layer_id[i] = i;
                    enccfg_.ts_rate_decimator[i] = numTLayers-i;
                }
//                enccfg_.ts_target_bitrate[0] = 300;
//                enccfg_.ts_target_bitrate[1] = 500;
//                enccfg_.ts_rate_decimator[0] = 2;
//                enccfg_.ts_rate_decimator[1] = 1;
//                enccfg_.temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_BYPASS;
                
                flagsOfTLayerLength_ = 0;
                flagsOfTLayer_[0] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF; // kTemporalUpdateLastAndGoldenRefAltRef;
                flagsOfTLayer_[1] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_LAST; // kTemporalUpdateGoldenWithoutDependencyRefAltRef;
                flagsOfTLayer_[2] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF ; // kTemporalUpdateLastRefAltRef;
                flagsOfTLayer_[3] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST; //kTemporalUpdateGoldenRefAltRef;
                flagsOfTLayer_[4] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_REF_GF; // kTemporalUpdateLastRefAltRef;
                flagsOfTLayer_[5] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST; //kTemporalUpdateGoldenRefAltRef;
                flagsOfTLayer_[6] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_REF_GF; // kTemporalUpdateLastRefAltRef;
                flagsOfTLayer_[7] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_GF  | VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY; // kTemporalUpdateNone;
                

//                //enccfg_.rc_dropframe_thresh = (unsigned int)strtoul(argv[9], NULL, 0);
//                enccfg_.rc_end_usage = VPX_CBR;
//                enccfg_.rc_min_quantizer = 2;
//                enccfg_.rc_max_quantizer = 56;
//                // if (strncmp(encoder->name, "vp9", 3) == 0) enccfg_.rc_max_quantizer = 52;
//                enccfg_.rc_undershoot_pct = 50;
//                enccfg_.rc_overshoot_pct = 50;
//                enccfg_.rc_buf_initial_sz = 600;
//                enccfg_.rc_buf_optimal_sz = 600;
//                enccfg_.rc_buf_sz = 1000;
//                
//                // Disable dynamic resizing by default.
//                enccfg_.rc_resize_allowed = 0;
                
                // Use 1 thread as default.
                //enccfg_.g_threads = 0;
                
            }
            
            //        enccfg.g_timebase.num = info.time_base.numerator;
            //        enccfg.g_timebase.den = info.time_base.denominator;
            //        enccfg.g_error_resilient = (vpx_codec_er_flags_t)strtoul(argv[7], NULL, 0);

//            encflags |= VPX_CODEC_USE_OUTPUT_PARTITION;
            vpxret = vpx_codec_enc_init_ver(&encctx_,
                                            vpx_codec_vp8_cx(),
                                            &enccfg_,
                                            encflags, VPX_ENCODER_ABI_VERSION);
            if(vpxret != VPX_CODEC_OK){
                odbge("vpx_codec_enc_init error with %d-[%s]", vpxret, vpx_codec_error(&encctx_));
                ret = -1;
                encctx_.name = NULL;
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
        if(encctx_.name){
            vpx_codec_destroy(&encctx_);
            encctx_.name = NULL;
        }
        this->freeEncodeBuf();
        numFrames_ = 0;
        flagsOfTLayerLength_ = 0;
        flagIndex_ = 0;
    }
    
    bool isTLayerEnabled(){
        bool e = (enccfg_.ts_number_layers <= 1 || enccfg_.ts_periodicity == 0);
        return !e;
    }
    int getTLayerId(){
        if(!this->isTLayerEnabled() || numFrames_ == 0){
            return 0;
        }
        int index = (numFrames_-1)%enccfg_.ts_periodicity;
        unsigned int layerId = enccfg_.ts_layer_id[index];
        return (int) layerId;
    }
    
    int nextTLayerId(){
        if(!this->isTLayerEnabled()){
            return 0;
        }
        int index = (numFrames_)%enccfg_.ts_periodicity;
        unsigned int layerId = enccfg_.ts_layer_id[index];
        return (int) layerId;
    }
    
    int nextTLayerFlag(){
        if(!this->isTLayerEnabled()){
            return 0;
        }
        if(flagsOfTLayerLength_ > 0){
            int idx = flagIndex_ % flagsOfTLayerLength_;
            ++flagIndex_;
            return this->flagsOfTLayer_[idx];
        }else{
            int layer_flags[2] = {0, 0};
            //layer_flags[0] |= VPX_EFLAG_FORCE_KF;
            layer_flags[0] |= (VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF );
            //layer_flags[0] |= VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF;
            layer_flags[1] |= (VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_REF_ARF);
            int idx = this->nextTLayerId();
            return layer_flags[idx];
//            return 0;

        }
    }
    
    int encode(vpx_image_t * vpximg, vpx_codec_pts_t pts, uint32_t duration, vpx_enc_frame_flags_t encflags = 0, bool forceKeyframe = false){
        vpx_codec_err_t vpxret = VPX_CODEC_OK;
        int ret = -1;
        do{
            frameLength_ = 0;
            frameFlags_ = 0;
            
            //uint32_t duration = 90000 / framerate_;
            //vpx_enc_frame_flags_t encflags = 0;
            if (forceKeyframe){
                encflags |= VPX_EFLAG_FORCE_KF;
            }
            if(this->isTLayerEnabled()){
                encflags |= this->nextTLayerFlag();
                //vpx_codec_control(&encctx_, VP8E_SET_FRAME_FLAGS, (int)frameflags);
                
                int layerId = this->nextTLayerId();
                vpx_codec_control(&encctx_, VP8E_SET_TEMPORAL_LAYER_ID, layerId);
            }
            
            vpxret = vpx_codec_encode(&encctx_, vpximg, pts, duration, encflags, VPX_DL_REALTIME);
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
            if(npkts > 0){
                ++numFrames_;
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
               , int * pCorrupted = NULL
               , int * pRefUsed = NULL){
        imageIter_ = NULL;
        vpx_codec_err_t vpxret = vpx_codec_decode(&decctx_, frameData, (unsigned int)frameLength, NULL, 0);
        if (vpxret != VPX_CODEC_OK) {
            odbge("vpx_codec_decode fail with %d", vpxret);
            return -1;
        }
        
        int refused = 0;
        vpxret = vpx_codec_control(&decctx_, VP8D_GET_LAST_REF_USED, &refused);
        if (vpxret != VPX_CODEC_OK) {
            odbge("vpx_codec_control VP8D_GET_LAST_REF_USED fail with %d", vpxret);
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
        //odbgi("decode: ref=0x%02x, corrupted=%d", refupdates, corrupted);
        
        if(pRefUpdate){
            *pRefUpdate = refupdates;
        }
        if(pCorrupted){
            *pCorrupted = corrupted;
        }
        if(pRefUsed){
            *pRefUsed = refused;
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


static
int makeTextCommon(VP8Encoder * encoder, int numFrames, char * txt){
    bool isKey = (encoder->getFrameFlags()&VPX_FRAME_IS_KEY)!=0;
    return sprintf(txt, " Frame %d, type=%c", numFrames, isKey?'I':'P');
}

struct User{
    SDLWindowImpl * window_ = NULL;
    SDLVideoView * videoView_ = NULL;
    SDLTextView * frameTxtView_ = NULL;
    User(const std::string& name, int w, int h)
    : window_(new SDLWindowImpl(name, w, h))
    , videoView_(new SDLVideoView())
    , frameTxtView_(new SDLTextView()){
        window_->addView(videoView_);
        window_->addView(frameTxtView_);
    }
    virtual ~User(){
        if(window_){
            delete window_;
            window_ = NULL;
        }
    }
};

struct EncodeUser : public User{
    VP8Encoder * encoder_ = NULL;
    EncodeUser(const std::string& name, int w, int h)
    : User(name, w, h)
    , encoder_(new VP8Encoder()){
        
    }
    virtual ~EncodeUser(){
        if(encoder_){
            delete encoder_;
            encoder_ = NULL;
        }
    }
};

struct DecodeUser : public User {
    VP8Decoder * decoder_ = NULL;
    int layerId_ = 0;
    int numFrames_ = 0;
    vpx_codec_pts_t lastPTS4FPS_ = 0;
    int lastNumFrames_  =0;
    int fps_ = 0;
    int64_t inputBytes_ = 0;
    int64_t bitrate_ = 0;
    DecodeUser(const std::string& name, int w, int h, int layerId = 0)
    : User(name, w, h)
    , decoder_(new VP8Decoder())
    , layerId_(layerId){
        
    }
    virtual ~DecodeUser(){
        if(decoder_){
            delete decoder_;
            decoder_ = NULL;
        }
    }
    
    void decode(vpx_codec_pts_t pts, VP8Encoder * encoder, int frameNo, int layerId, bool drop){

        if(layerId > layerId_){
            return ;
        }
        
        if(pts < lastPTS4FPS_){
            lastPTS4FPS_ = pts;
            lastNumFrames_ = 0;
            fps_ = 0;
            numFrames_ = 0;
            inputBytes_ = 0;
            bitrate_ = 0;
        }
        
        int ret = 0;
        int refupdate = 0;
        int corrupted = 0;
        int refUsed = 0;
        const uint8_t * frameData = NULL;
        size_t frameLength = 0;
        if(!drop){
            frameData = encoder->getEncodedFrame();
            frameLength = encoder->getEncodedLength();
        }
        inputBytes_ += frameLength;
        ret = decoder_->decode(frameData, frameLength, &refupdate, &corrupted, &refUsed);
        if (ret == 0) {
            vpx_image_t * decimg = NULL;
            while ((decimg = decoder_->pullImage()) != NULL) {
                ++numFrames_;
                vpx_codec_pts_t duration = pts - lastPTS4FPS_;
                int num = numFrames_ - lastNumFrames_;
                if(duration >= 90000*2 && num > 0 && inputBytes_ > 0){
                    fps_ = 90000* num / duration;
                    bitrate_ = 90000 * inputBytes_*8 /duration/1000;
                    lastNumFrames_ = numFrames_;
                    lastPTS4FPS_ = pts;
                    inputBytes_ = 0;
                }
                
                videoView_->drawYUV(decimg->d_w, decimg->d_h
                                            , decimg->planes[0], decimg->stride[0]
                                            , decimg->planes[1], decimg->stride[1]
                                            , decimg->planes[2], decimg->stride[2]);
                char txt[128];
                int txtlen = makeTextCommon(encoder, frameNo, txt+0);
                txtlen += sprintf(txt+txtlen, ", TL=%d", layerId );
                txtlen += sprintf(txt+txtlen, ", fps=%d", fps_ );
                txtlen += sprintf(txt+txtlen, ", br=%lld", bitrate_ );
                if(corrupted){
                    txtlen += sprintf(txt+txtlen, ",corrupted" );
                }
                txtlen += sprintf(txt+txtlen, ", upd(%s %s %s)"
                                  ,(refupdate & VP8_LAST_FRAME)?"LF":""
                                  ,(refupdate & VP8_GOLD_FRAME)?"GF":""
                                  ,(refupdate & VP8_ALTR_FRAME)?"ALF":"");
                txtlen += sprintf(txt+txtlen, ", ref(%s %s %s)"
                                  ,(refUsed & VP8_LAST_FRAME)?"LF":""
                                  ,(refUsed & VP8_GOLD_FRAME)?"GF":""
                                  ,(refUsed & VP8_ALTR_FRAME)?"ALF":"");
                
                frameTxtView_->draw(txt);
                odbgi("  decode=[%s]", txt);
            }
        }
    }
};


class AppDemo{
    vpx_image_t * vpximg_ = NULL;
    
    EncodeUser * encodeUser_ = NULL;
    DecodeUser * decodeUser1_ = NULL;
    DecodeUser * decodeUser2_ = NULL;
    
    std::string fileName_;
    FILE * fp_ = NULL;
    int numFrames_ = 0;
    size_t numEncodeBytes_ = 0;
    int framerate_ = 0;
    int keyInterval_ = 0;
    uint32_t startTime_ = 0;
    bool drop_ = false;
    vpx_codec_pts_t pts_ = 0;
    uint32_t duration_ = 90000 / 30;
public:
    virtual ~AppDemo(){
        this->close();
    }
    
    int start(){
        if(fileName_.empty()){
            return -1;
        }
        if(fp_){
            return 0;
        }
        int ret = 0;
        fp_ = fopen(fileName_.c_str(), "rb");
        if (!fp_) {
            odbge("fail to open yuv file [%s]", fileName_.c_str());
            ret = -1;
        }
        return ret;
    }
    
    void stop(){
        if(fp_){
            fclose(fp_);
            fp_ = NULL;
            
            uint32_t elapsed = SDL_GetTicks() - startTime_;
            if(numFrames_ > 0 && elapsed > 0){
                odbgi("final: elapsed=%d ms, frames=%d, encBytes=%zu, fps=%d, bps=%zu"
                      , elapsed, numFrames_, numEncodeBytes_, 1000*numFrames_/elapsed, 8*numEncodeBytes_*framerate_/numFrames_);
            }
        }
        numFrames_ = 0;
        numEncodeBytes_ = 0;
        drop_ = false;
        pts_ = 0;
    }
    
    void close(){
        this->stop();
        if(encodeUser_){
            delete encodeUser_;
            encodeUser_ = NULL;
        }
        if(decodeUser1_){
            delete decodeUser1_;
            decodeUser1_ = NULL;
        }
        if(decodeUser2_){
            delete decodeUser2_;
            decodeUser2_ = NULL;
        }
        if(vpximg_){
            vpx_img_free(vpximg_);
            vpximg_ = NULL;
        }
        fileName_.clear();
    }
    
    int open(const char * yuvfile, int width, int height, int framerate, int keyInterval = 0){
        int ret = -1;
        do{
            this->close();
            
            fileName_ = yuvfile;
            framerate_ = framerate;
            keyInterval_ = keyInterval;
            duration_ = 90000 / framerate;
            
            encodeUser_ = new EncodeUser("raw", width, height);
            decodeUser1_ = new DecodeUser("decode1", width, height, 1);
            decodeUser2_ = new DecodeUser("decode2", width, height, 0);
            
            int bitrateKbps = 500;
            ret = encodeUser_->encoder_->open(width, height, framerate, bitrateKbps, 2);
            if(ret) break;
            
            ret = decodeUser1_->decoder_->open();
            if(ret) break;
            
            ret = decodeUser2_->decoder_->open();
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

            encodeUser_->window_->open(x1, y);
            if(ret) break;

            decodeUser1_->window_->open(x2, y-height/2);
            if(ret) break;
            
            decodeUser2_->window_->open(x2, y+height/2);

            encodeUser_->frameTxtView_->draw(" press 'i' -> Key Frame, press 'p' -> P Frame, 'ctrl+' -> drop");
            decodeUser1_->frameTxtView_->draw(" press 'g' -> Play/Pause");
            decodeUser2_->frameTxtView_->draw(" press 'q' -> Quit");
            
            vpximg_ = vpx_img_alloc(NULL, VPX_IMG_FMT_I420, width, height, 1);
            if(!vpximg_){
                ret = -1;
                break;
            }
            
            ret = this->start();
            if(ret) break;
            
            ret = 0;
        }while(0);
        if(ret){
            this->close();
        }
        return ret;
    }
    
    bool isProcessing(){
        return fp_?true : false;
    }
    
    int processOneFrame(bool enckey, bool drop){
        int ret = 0;
        do{
            if(!fp_){
                break;
            }
            ret = vpx_img_read(vpximg_, fp_);
            if(ret < 0){
                if(numFrames_ > 0){
                    this->stop();
                    ret = 0;
                }
                break;
            }
            if(numFrames_ == 0){
                startTime_ = SDL_GetTicks();
                pts_ = 0;
            }else{
                pts_ += duration_;
            }
            
            odbgi("--- enc frame %d", numFrames_);
            encodeUser_->videoView_->drawYUV(vpximg_->d_w, vpximg_->d_h
                                 , vpximg_->planes[0], vpximg_->stride[0]
                                 , vpximg_->planes[1], vpximg_->stride[1]
                                 , vpximg_->planes[2], vpximg_->stride[2]);

            int layerId = 0;
            vpx_enc_frame_flags_t encflags = 0;
            if(!encodeUser_->encoder_->isTLayerEnabled()){
                if(numFrames_ % 2 == 0){
                    layerId = 0;
                    //encflags |= VPX_EFLAG_FORCE_KF;
                    encflags |= (VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF );
//                    encflags |= VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF;
                    
                }else{
                    layerId = 1;
                    encflags |= (VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_REF_ARF);
                }
            }
            
            ret = encodeUser_->encoder_->encode(vpximg_, pts_, duration_, enckey, encflags);
            if(ret){
                break;
            }
            ++numFrames_;
            if(encodeUser_->encoder_->isTLayerEnabled()){
                layerId = encodeUser_->encoder_->getTLayerId();
            }
            
            
            char txt[128];
            int txtlen = ::makeTextCommon(encodeUser_->encoder_, numFrames_, txt+0);
            
            if(drop){
                txtlen += sprintf(txt+txtlen, ", drop=%d", drop);
            }
            encodeUser_->frameTxtView_->draw(txt);

            numEncodeBytes_ += encodeUser_->encoder_->getEncodedLength();
            if(encodeUser_->encoder_->getEncodedLength() > 0){
                dump_vp8_frame(encodeUser_->encoder_->getEncodedFrame(), encodeUser_->encoder_->getEncodedLength());
                decodeUser1_->decode(pts_, encodeUser_->encoder_, numFrames_, layerId, drop);
                decodeUser2_->decode(pts_, encodeUser_->encoder_, numFrames_, layerId, drop);
            }
            ret = 0;
        }while(0);
        return ret;
    }
    
    void refresh(){
        if(encodeUser_){
            encodeUser_->window_->refresh();
        }
        if(decodeUser1_){
            decodeUser1_->window_->refresh();
        }
        if(decodeUser2_){
            decodeUser2_->window_->refresh();
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
        
        int frameInterval = 1000 / framerate;
        bool playing = false;
        uint32_t nextDrawTime = SDL_GetTicks();
        bool quitLoop = false;
        SDL_Event event;
        while(!quitLoop){
            int timeout = 10;
            uint32_t now_ms = SDL_GetTicks();
            if(playing){
                if(now_ms >= nextDrawTime){
                    ret = app->processOneFrame(false, false);
                    if(ret) break;
                    nextDrawTime += frameInterval;
                    if(!app->isProcessing()){
                        playing = false;
                    }
                }
                
                now_ms = SDL_GetTicks();
                if(now_ms < nextDrawTime){
                    timeout = (int)(nextDrawTime - now_ms);
                }
                
            }
            if(timeout > 0){
                SDL_WaitEventTimeout(NULL, timeout);
            }
            
            while (SDL_PollEvent(&event)) {
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
                    }else if(event.key.keysym.sym == SDLK_g){
                        playing = !playing;
                        if(playing){
                            app->start();
                            nextDrawTime = SDL_GetTicks();
                        }
                    }else if(event.key.keysym.sym == SDLK_q){
                        quitLoop = true;
                    }
                    if(ret){
                        quitLoop = true;
                    }
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
