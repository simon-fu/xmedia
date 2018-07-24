
#include <string>
#include <map>
#include <vector>
#include <list>
#include <chrono>
#include <stdio.h>
extern "C"{
//    #include "SDL2/SDL.h"
//    #include "SDL2/SDL_ttf.h"
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

static
int64_t get_timestamp_ms(){
    unsigned long milliseconds_since_epoch =
    std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count();
    return milliseconds_since_epoch;
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
int vpx_img_write(vpx_image_t *img, FILE *file) {
    int factor = ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    for (int plane = 0; plane < 3; ++plane) {
        unsigned char *buf = img->planes[plane];
        const int stride = img->stride[plane];
        const int w = vpx_img_plane_width(img, plane) * factor;
        const int h = vpx_img_plane_height(img, plane);
        int y;
        
        for (y = 0; y < h; ++y) {
            if (fwrite(buf, 1, w, file) != (size_t)w){
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
int encode_decode_vp8(const char * inputYUVFile, int width, int height, int framerate, const char * outputYUVFile){
    int ret = 0;
    VP8Encoder * encoder_ = NULL;
    VP8Decoder * decoder_ = NULL;
    vpx_image_t * vpximg_ = NULL;
    FILE * fp_ = NULL;
    FILE * outfp_ = NULL;

    do{
        int keyInterval_ = framerate > 0 ? framerate : 25;
        
        encoder_ = new VP8Encoder();
        decoder_ = new VP8Decoder();
        
        fp_ = fopen(inputYUVFile, "rb");
        if (!fp_) {
            odbge("fail to open yuv file [%s]", inputYUVFile);
            ret = -1;
            break;
        }
        
        outfp_ = fopen(outputYUVFile, "wb");
        if(!outfp_){
            odbge("fail to write open [%s]", outputYUVFile);
            ret = -1;
            break;
        }
        
        int bitrateKbps = 500;
        ret = encoder_->open(width, height, framerate, bitrateKbps);
        if(ret) break;
        
        ret = decoder_->open();
        if(ret) break;
        
        vpximg_ = vpx_img_alloc(NULL, VPX_IMG_FMT_I420, width, height, 1);
        if(!vpximg_){
            ret = -1;
            break;
        }
        
        int numFrames_ = 0;
        size_t numEncodeBytes_ = 0;
        int64_t startTime_ = get_timestamp_ms();
        while(1){
            ret = vpx_img_read(vpximg_, fp_);
            if(ret < 0){
                break;
            }
            bool forceKey = (numFrames_ % keyInterval_ == 0);
            odbgi("--- enc frame %d, forceKey=%d", numFrames_, forceKey);
            ret = encoder_->encode(vpximg_, forceKey);
            if(ret){
                break;
            }
            numEncodeBytes_ += encoder_->getEncodedLength();
            bool isDecodedFrame = false;
            if(encoder_->getEncodedLength() > 0){
                dump_vp8_frame(encoder_->getEncodedFrame(), encoder_->getEncodedLength());
                ret = decoder_->decode(encoder_->getEncodedFrame(), encoder_->getEncodedLength());
                if (ret == 0) {
                    vpx_image_t * decimg = NULL;
                    while ((decimg = decoder_->pullImage()) != NULL) {
                        vpx_img_write(decimg, outfp_);
                        isDecodedFrame = true;
                    }
                }
            }
            if(isDecodedFrame){
                ++numFrames_;
            }
        }
        int64_t elapsed = get_timestamp_ms() - startTime_;
        if(numFrames_ > 0 && elapsed > 0){
            odbgi("elapsed=%lld, frames=%d, fps=%d, encodedBytes=%zu", elapsed, numFrames_, (int)(1000*numFrames_/elapsed), numEncodeBytes_);
            odbgi("ffplay -f rawvideo -pixel_format yuv420p -video_size %dx%d -framerate %d %s", width, height, framerate, outputYUVFile);
        }
        
        ret = 0;
    }while(0);
    
    if(encoder_){
        delete encoder_;
        encoder_ = NULL;
    }
    
    if(decoder_){
        delete decoder_;
        decoder_ = NULL;
    }
    
    if(vpximg_){
        vpx_img_free(vpximg_);
        vpximg_ = NULL;
    }
    
    if(fp_){
        fclose(fp_);
        fp_ = NULL;
    }
    
    if(outfp_){
        fclose(outfp_);
        outfp_ = NULL;
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

int main(int argc, char* argv[]){
    int ret = 0;
    std::string inputFile = getFilePath("../data/test_yuv420p_640x360.yuv");
    std::string outputFile = getFilePath("../data/out_yuv420p_640x360.yuv");
    ret = encode_decode_vp8(inputFile.c_str(), 640, 360, 25, outputFile.c_str());
    return ret;
}
