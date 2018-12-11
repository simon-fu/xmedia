

#ifndef VP8Framework_hpp
#define VP8Framework_hpp

#include <stdio.h>
#include <string>
extern "C"{
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"
}


class VP8LayerStrategy{
    int numTLayers_ = 1;
    int flagsOfTLayer_[16];
    int flagsOfTLayerLength_ = 0;
    int flagIndex_ = -1;
    int layerIdIndex_ = -1;
    bool isSettingLayerId_ = false;
    
    void configTLayersCommon(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps);
    
    void config2Layers8FramePeriod1(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps);
    
    void config2Layers8FramePeriod2(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps);
    
    void config2Layers2FramePeriod(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps);
    
    void configSingleLayer(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps);
    
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
    
    int open(int width, int height, const vpx_rational& timebase, int bitrateKbps, int numTLayers = 1);
    
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
    
    
    int encode(vpx_image_t * vpximg, vpx_codec_pts_t pts, uint32_t duration, vpx_enc_frame_flags_t encflags = 0, bool forceKeyframe = false);
    
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
               , int * pRefUsed = NULL);
    
    vpx_image_t * pullImage(){
        vpx_image_t * decimg = vpx_codec_get_frame(&decctx_, &imageIter_);
        return decimg;
    }
};

#endif /* VP8Framework_hpp */
