

#include "VP8Framework.hpp"

#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "VP8 ", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "VP8 ", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "VP8 ", ##ARGS); printf("\n"); fflush(stdout); }while(0)


void VP8LayerStrategy::configTLayersCommon(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps){
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

void VP8LayerStrategy::config2Layers8FramePeriod1(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps){
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

void VP8LayerStrategy::config2Layers8FramePeriod2(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps){
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

void VP8LayerStrategy::config2Layers2FramePeriod(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps){
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

void VP8LayerStrategy::configSingleLayer(vpx_codec_enc_cfg_t &enccfg, int bitrateKbps){
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


int VP8Encoder::open(int width, int height, const vpx_rational& timebase, int bitrateKbps, int numTLayers){
    
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

int VP8Encoder::encode(vpx_image_t * vpximg, vpx_codec_pts_t pts, uint32_t duration, vpx_enc_frame_flags_t encflags , bool forceKeyframe){
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


int VP8Decoder::decode(const uint8_t * frameData, size_t frameLength
           , int * pRefUpdate
           , int * pCorrupted
           , int * pRefUsed ){
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


