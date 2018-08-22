
#include <string>
#include <map>
#include <vector>
#include <list>
#include <stdio.h>
extern "C"{
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

#include "SDLFramework.hpp"

#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)

class VP8LayerStrategy{
    int numTLayers_ = 1;
    int flagsOfTLayer_[16];
    int flagsOfTLayerLength_ = 0;
    int flagIndex_ = -1;
    int layerIdIndex_ = -1;
    bool isSettingLayerId_ = false;
    
    void configTLayersCommon(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps){
        enccfg.g_error_resilient = 1;
        enccfg.rc_dropframe_thresh = 30;
        enccfg.rc_end_usage = VPX_CBR;
        enccfg.rc_min_quantizer = 2;
        enccfg.rc_max_quantizer = 56;
        //enccfg.rc_max_quantizer = 52; // VP9
        enccfg.rc_undershoot_pct = 50;
        enccfg.rc_overshoot_pct = 50;
        enccfg.rc_buf_initial_sz = 600;
        enccfg.rc_buf_optimal_sz = 600;
        enccfg.rc_buf_sz = 1000;
        enccfg.rc_resize_allowed = 0;
        enccfg.g_threads = 1;
        enccfg.g_lag_in_frames = 0;
        enccfg.kf_mode = VPX_KF_AUTO;
        enccfg.kf_min_dist = enccfg.kf_max_dist = 3000;
        enccfg.temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_BYPASS;
        enccfg.rc_target_bitrate = bitrateKbps;
        isSettingLayerId_ = true;
    }
    
    void config2Layers8FramePeriod1(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps){
        // from webrtc
        odbgi("config2Layers8FramePeriod1: br = %d kbps", bitrateKbps);
        numTLayers_ = 2;
        enccfg.ts_number_layers = numTLayers_;
        enccfg.ts_periodicity = numTLayers_;
        int br = bitrateKbps / numTLayers_;
        for(int i = 0; i < numTLayers_; ++i){
            enccfg.ts_layer_id[i] = i;
            enccfg.ts_target_bitrate[i] = (i+1)*br;
            enccfg.ts_rate_decimator[i] = numTLayers_-i;
        }
        flagsOfTLayerLength_ = 8;
        flagsOfTLayer_[0] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF; // kTemporalUpdateLastAndGoldenRefAltRef;
        flagsOfTLayer_[1] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_LAST; // kTemporalUpdateGoldenWithoutDependencyRefAltRef;
        flagsOfTLayer_[2] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF ; // kTemporalUpdateLastRefAltRef;
        flagsOfTLayer_[3] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST; //kTemporalUpdateGoldenRefAltRef;
        flagsOfTLayer_[4] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_REF_GF; // kTemporalUpdateLastRefAltRef;
        flagsOfTLayer_[5] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST; //kTemporalUpdateGoldenRefAltRef;
        flagsOfTLayer_[6] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_REF_GF; // kTemporalUpdateLastRefAltRef;
        flagsOfTLayer_[7] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_GF  | VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY; // kTemporalUpdateNone;
        
        this->configTLayersCommon(enccfg, bitrateKbps);
    }
    
    void config2Layers8FramePeriod2(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps){
        // from https://github.com/webmproject/libvpx/blob/master/examples/vpx_temporal_svc_encoder.c
        odbgi("config2Layers8FramePeriod2: br = %d kbps", bitrateKbps);
        numTLayers_ = 2;
        enccfg.ts_number_layers = numTLayers_;
        enccfg.ts_periodicity = numTLayers_;
        int br = bitrateKbps / numTLayers_;
        for(int i = 0; i < numTLayers_; ++i){
            enccfg.ts_target_bitrate[i] = (i+1)*br;
            enccfg.ts_layer_id[i] = i;
            enccfg.ts_rate_decimator[i] = numTLayers_-i;
        }
        
        
        flagsOfTLayerLength_ = 8;
        
        // TODO: removing VPX_EFLAG_FORCE_KF is OK
        // Layer 0: predict from L and ARF, update L and G.
        flagsOfTLayer_[0] = VPX_EFLAG_FORCE_KF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_ARF;
        // Layer 1: sync point: predict from L and ARF, and update G.
        flagsOfTLayer_[1] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ARF;
        // Layer 0, predict from L and ARF, update L.
        flagsOfTLayer_[2] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
        // Layer 1: predict from L, G and ARF, and update G.
        flagsOfTLayer_[3] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY;
        // Layer 0.
        flagsOfTLayer_[4] = flagsOfTLayer_[2];
        // Layer 1.
        flagsOfTLayer_[5] = flagsOfTLayer_[3];
        // Layer 0.
        flagsOfTLayer_[6] = flagsOfTLayer_[4];
        // Layer 1.
        flagsOfTLayer_[7] = flagsOfTLayer_[5];
        
        this->configTLayersCommon(enccfg, bitrateKbps);
    }
    
    void config2Layers2FramePeriod(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps){
        odbgi("config2Layers2FramePeriod: br = %d kbps", bitrateKbps);
        
        numTLayers_ = 2;
        const float bitrateFactors[] = {0.6f, 1.0f};
        enccfg.ts_number_layers = numTLayers_;
        enccfg.ts_periodicity = numTLayers_;
        //int br = bitrateKbps / numTLayers_;
        for(int i = 0; i < numTLayers_; ++i){
            enccfg.ts_layer_id[i] = i;
            //enccfg.ts_target_bitrate[i] = (i+1)*br;
            enccfg.ts_target_bitrate[i] = bitrateFactors[i] * bitrateKbps;
            enccfg.ts_rate_decimator[i] = numTLayers_-i;
        }
        enccfg.ts_target_bitrate[0] = bitrateKbps * 0.6f;
        enccfg.ts_target_bitrate[1] = bitrateKbps;
        enccfg.ts_rate_decimator[0] = 2;
        enccfg.ts_rate_decimator[1] = 1;
        
        flagsOfTLayerLength_ = 2;
        
        //flagsOfTLayer_[0] |= VPX_EFLAG_FORCE_KF;
        flagsOfTLayer_[0] |= (VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF );
        //flagsOfTLayer_[0] |= VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF;
        
        flagsOfTLayer_[1] |= (VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_REF_ARF);
        this->configTLayersCommon(enccfg, bitrateKbps);
        
    }
    
    void configSingleLayer(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps){
        odbgi("configSingleLayer: br = %d kbps", bitrateKbps);
        numTLayers_ = 1;
        enccfg.ts_number_layers = numTLayers_;
        enccfg.rc_end_usage = VPX_CBR;
        enccfg.rc_dropframe_thresh = 30;
        enccfg.rc_min_quantizer = 2;
        enccfg.rc_max_quantizer = 56;
        enccfg.rc_undershoot_pct = 15;
        enccfg.rc_overshoot_pct = 15;
        enccfg.rc_buf_initial_sz = 500;
        enccfg.rc_buf_optimal_sz = 600;
        enccfg.rc_buf_sz = 1000;
        isSettingLayerId_ = false;
    }
    
public:
    VP8LayerStrategy(){
    }
    
    virtual ~VP8LayerStrategy(){
        
    }
    
    void reset(){
        numTLayers_ = 1;
        flagsOfTLayerLength_ = 0;
        flagIndex_ = -1;
        layerIdIndex_ = -1;
        memset(flagsOfTLayer_, 0, sizeof(flagsOfTLayer_));
    }
    
    void config(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps){
        this->reset();
//        this->configSingleLayer(enccfg, bitrateKbps);
        this->config2Layers2FramePeriod(enccfg, bitrateKbps);
//        this->config2Layers8FramePeriod1(enccfg, bitrateKbps);
//        this->config2Layers8FramePeriod2(enccfg, bitrateKbps);
    }
    
    void next(vpx_enc_frame_flags_t &encflags, int &layerId){
        ++layerIdIndex_;
        layerIdIndex_ = layerIdIndex_ % numTLayers_;
        layerId = layerIdIndex_;
        
        if(flagsOfTLayerLength_ > 0){
            ++flagIndex_;
            flagIndex_ = flagIndex_ % flagsOfTLayerLength_;
            encflags |= (this->flagsOfTLayer_[flagIndex_]);
        }
    }
    
    int layerId(){
        return layerIdIndex_;
    }
    
    bool isSettingLayerId(){
        return isSettingLayerId_;
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
    
    VP8LayerStrategy layerStrategy_;
    
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
    
    int open(int width, int height, const vpx_rational& timebase, int bitrateKbps, int numTLayers = 1){
        
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
            //framerate_ = framerate;
            
            vpxret = vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &enccfg_, 0);
            enccfg_.g_w = width_;
            enccfg_.g_h = height_;
            enccfg_.rc_target_bitrate = bitrateKbps_; // in unit of kbps
            enccfg_.g_timebase = timebase;
            
//            enccfg_.kf_mode = VPX_KF_AUTO;
            
            layerStrategy_.config(enccfg_, bitrateKbps);
            

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
            
            // for VP8 
            //vpx_codec_control(&encctx_, VP8E_SET_CPUUSED, -speed);
            vpx_codec_control(&encctx_, VP8E_SET_NOISE_SENSITIVITY, 0);
            vpx_codec_control(&encctx_, VP8E_SET_STATIC_THRESHOLD, 1);
            vpx_codec_control(&encctx_, VP8E_SET_GF_CBR_BOOST_PCT, 0);
            
            
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
    
//    bool isTLayerEnabled(){
//        bool e = (enccfg_.ts_number_layers <= 1 || enccfg_.ts_periodicity == 0);
//        return !e;
//    }
    
    int getTLayerId(){
        return layerStrategy_.layerId();
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
            int layerId = 0;
            layerStrategy_.next(encflags, layerId);
            if(layerStrategy_.isSettingLayerId()){
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
    if(encoder->getEncodedLength() > 0){
        bool isKey = (encoder->getFrameFlags()&VPX_FRAME_IS_KEY)!=0;
        return sprintf(txt, " Frame %d, type=%c", numFrames, isKey?'I':'P');
    }else{
        return sprintf(txt, " Frame %d, drop=1", numFrames);
    }
    
}

class VideoStati{
    vpx_rational timebase_;
    vpx_codec_pts_t pts_ = 0;
    int numFrames_ = 0;
    vpx_codec_pts_t lastPTS4FPS_ = 0;
    int lastNumFrames_  =0;
    int fps_ = 0;
    int64_t inputBytes_ = 0;
    int64_t bitrate_ = 0;
public:
    VideoStati(const vpx_rational& timebase){
        timebase_ = timebase;
        this->reset(0);
    }
    
    void reset(vpx_codec_pts_t pts){
        lastPTS4FPS_ = pts;
        lastNumFrames_ = 0;
        fps_ = 0;
        numFrames_ = 0;
        inputBytes_ = 0;
        bitrate_ = 0;
    }
    
    void processBytes(size_t bytes){
        inputBytes_ += bytes;
    }
    
    bool check(vpx_codec_pts_t pts, int newNumFrames, size_t bytes){
        if(pts < lastPTS4FPS_){
            this->reset(pts);
        }
        pts_ = pts;
        numFrames_ += newNumFrames;
        inputBytes_ += bytes;
        
        vpx_codec_pts_t duration_ms = 1000 * (pts_-lastPTS4FPS_) * timebase_.num/timebase_.den;
        int num = numFrames_ - lastNumFrames_;
        if(duration_ms >= 2000 && num > 0 && inputBytes_ > 0){
            fps_ = 1000* num / duration_ms;
            bitrate_ = 1000 * inputBytes_*8 /duration_ms;
            lastNumFrames_ = numFrames_;
            lastPTS4FPS_ = pts_;
            inputBytes_ = 0;
            return true;
        }
        return false;
    }
    
    int getFPS(){
        return fps_;
    }
    
    int64_t getBitrate(){
        return bitrate_;
    }
};

struct User{
    SDLWindowImpl * window_ = NULL;
    SDLVideoView * videoView_ = NULL;
    SDLTextView * frameTxtView_ = NULL;
    VideoStati videoStati_;
    User(const std::string& name, int w, int h, const vpx_rational& timebase)
    : window_(new SDLWindowImpl(name, w, h))
    , videoView_(new SDLVideoView())
    , frameTxtView_(new SDLTextView())
    , videoStati_(timebase){
        window_->addView(videoView_);
        window_->addView(frameTxtView_);
    }
    virtual ~User(){
        if(window_){
            delete window_;
            window_ = NULL;
        }
    }
    
    virtual void reset(){
        videoStati_.reset(0);
    }
    
};

struct EncodeUser : public User{
    VP8Encoder * encoder_ = NULL;
    EncodeUser(const std::string& name, int w, int h, const vpx_rational& timebase)
    : User(name, w, h, timebase)
    , encoder_(new VP8Encoder()){
        
    }
    virtual ~EncodeUser(){
        if(encoder_){
            delete encoder_;
            encoder_ = NULL;
        }
    }
    int encode(vpx_image_t * vpximg, vpx_codec_pts_t pts, uint32_t duration, bool enckey, int frameNo, bool skip){
        videoView_->drawYUV(vpximg->d_w, vpximg->d_h
                                         , vpximg->planes[0], vpximg->stride[0]
                                         , vpximg->planes[1], vpximg->stride[1]
                                         , vpximg->planes[2], vpximg->stride[2]);
        
        int ret = encoder_->encode(vpximg, pts, duration, enckey);
        if(ret){
            return ret;
        }
        
        int layerId = encoder_->getTLayerId();
        videoStati_.check(pts, 1, encoder_->getEncodedLength());
        
        char txt[128];
        int txtlen = ::makeTextCommon(encoder_, frameNo, txt+0);
        txtlen += sprintf(txt+txtlen, ", TL=%d", layerId );
        txtlen += sprintf(txt+txtlen, ", fps=%d", videoStati_.getFPS() );
        txtlen += sprintf(txt+txtlen, ", br=%lld", videoStati_.getBitrate()/1000 );
        if(skip){
            txtlen += sprintf(txt+txtlen, ", skip=%d", skip);
        }
        frameTxtView_->draw(txt);
        return 0;
    }
};

struct DecodeUser : public User {
    VP8Decoder * decoder_ = NULL;
    int layerId_ = 0;
    DecodeUser(const std::string& name, int w, int h, const vpx_rational& timebase, int layerId = 0)
    : User(name, w, h, timebase)
    , decoder_(new VP8Decoder())
    , layerId_(layerId){
        
    }
    virtual ~DecodeUser(){
        if(decoder_){
            delete decoder_;
            decoder_ = NULL;
        }
    }
    
    void decode(vpx_codec_pts_t pts, VP8Encoder * encoder, int frameNo, int layerId, bool skip){

        if(layerId > layerId_){
            return ;
        }
        
        int ret = 0;
        int refupdate = 0;
        int corrupted = 0;
        int refUsed = 0;
        const uint8_t * frameData = NULL;
        size_t frameLength = 0;
        if(!skip){
            frameData = encoder->getEncodedFrame();
            frameLength = encoder->getEncodedLength();
        }
        
        videoStati_.processBytes(frameLength);
        ret = decoder_->decode(frameData, frameLength, &refupdate, &corrupted, &refUsed);
        if (ret == 0) {
            vpx_image_t * decimg = NULL;
            while ((decimg = decoder_->pullImage()) != NULL) {
                videoStati_.check(pts, 1, 0);
                
                videoView_->drawYUV(decimg->d_w, decimg->d_h
                                            , decimg->planes[0], decimg->stride[0]
                                            , decimg->planes[1], decimg->stride[1]
                                            , decimg->planes[2], decimg->stride[2]);
                char txt[128];
                int txtlen = makeTextCommon(encoder, frameNo, txt+0);
                txtlen += sprintf(txt+txtlen, ", TL=%d", layerId );
                txtlen += sprintf(txt+txtlen, ", fps=%d", videoStati_.getFPS() );
                txtlen += sprintf(txt+txtlen, ", br=%lld", videoStati_.getBitrate()/1000 );
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
    vpx_rational timebase_;
public:
    virtual ~AppDemo(){
        this->close();
    }
    
    int start(){
        if(fileName_.empty()){
            return -1;
        }
        int ret = 0;
        if(!fp_){
            fp_ = fopen(fileName_.c_str(), "rb");
            if (!fp_) {
                odbge("fail to open yuv file [%s]", fileName_.c_str());
                ret = -1;
            }
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
        if(encodeUser_){
            encodeUser_->reset();
        }
        if(decodeUser1_){
            decodeUser1_->reset();
        }
        if(decodeUser2_){
            decodeUser2_->reset();
        }
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
    
    bool isProcessing(){
        return fp_?true : false;
    }
    
    int open(const char * yuvfile, int width, int height, int framerate, int keyInterval = 0){
        int ret = -1;
        do{
            this->close();
            
            fileName_ = yuvfile;
            framerate_ = framerate;
            keyInterval_ = keyInterval;
            
//            timebase_.num = 1;
//            timebase_.den = 90000;
//            duration_ = 90000 / framerate;
            
            timebase_.num = 1;
            timebase_.den = framerate;
            duration_ = 1;
            
            encodeUser_ = new EncodeUser("raw", width, height, timebase_);
            decodeUser1_ = new DecodeUser("decode1", width, height, timebase_, 1);
            decodeUser2_ = new DecodeUser("decode2", width, height, timebase_, 0);
            
            int bitrateKbps = 600;
            ret = encodeUser_->encoder_->open(width, height, timebase_, bitrateKbps, 2);
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
    
    int processOneFrame(bool enckey, bool skip){
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
            
            ++numFrames_;
            odbgi("--- enc frame %d", numFrames_);
            ret = encodeUser_->encode(vpximg_, pts_, duration_, enckey, numFrames_, skip);
            if(ret){
                break;
            }
            int layerId = encodeUser_->encoder_->getTLayerId();

            numEncodeBytes_ += encodeUser_->encoder_->getEncodedLength();
            if(encodeUser_->encoder_->getEncodedLength() > 0){
                dump_vp8_frame(encodeUser_->encoder_->getEncodedFrame(), encodeUser_->encoder_->getEncodedLength());
                decodeUser1_->decode(pts_, encodeUser_->encoder_, numFrames_, layerId, skip);
                decodeUser2_->decode(pts_, encodeUser_->encoder_, numFrames_, layerId, skip);
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
