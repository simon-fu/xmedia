


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <chrono>
// for open h264 raw file.
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// #include "../../objs/include/srs_librtmp.h"
#include "srs_librtmp.h"
#include "Lab030-RTMPPush.hpp"

// see https://github.com/ossrs/srs/wiki/v2_CN_SrsLibrtmp
// see https://blog.csdn.net/win_lin/article/details/41170653
// see https://github.com/ossrs/srs/blob/master/trunk/research/librtmp/srs_h264_raw_publish.c
// see https://github.com/ossrs/srs/blob/master/trunk/research/librtmp/srs_aac_raw_publish.c
// see http://winlinvip.github.io/srs.release/3rdparty/720p.h264.raw
// see http://winlinvip.github.io/srs.release/3rdparty/audio.raw.aac



#define dbgd(...) srs_human_verbose(__VA_ARGS__)
#define dbgi(...) srs_human_trace(__VA_ARGS__)
#define dbge(...) srs_human_error(__VA_ARGS__)
//#define dbgi(...) do{printf(__VA_ARGS__); printf("\n");}while(0)


static
int found_nalu_annex(uint8_t * data, size_t data_size, size_t& pos, size_t& nalu_pos){
    int found_annex = 0;
    size_t last_check_pos = (data_size-4);
    for(;pos < last_check_pos; ++pos){
        if(data[pos+0] == 0 && data[pos+1] == 0){
            if(data[pos+2] == 1){
                nalu_pos = pos+3;
                found_annex = 1;
                break;
            }else if(data[pos+2] == 0 && data[pos+3] == 1){
                nalu_pos = pos+4;
                found_annex = 1;
                break;
            }
        }
    }
    if(!found_annex){
        if(pos == last_check_pos){
            ++pos;
            if(data[pos+0] == 0 && data[pos+1] == 0 && data[pos+2] == 1){
                nalu_pos = pos+3;
                found_annex = 1;
                return found_annex;
            }
        }
        pos = data_size;
        nalu_pos = pos;
    }
    return found_annex;
}

static
int read_h264_next_nalu(uint8_t * data, size_t data_size, size_t& pos, size_t &length, size_t& nalu_pos){
    
    pos += length;
    if(pos >= data_size){
        return -1;
    }
    
    size_t start_pos = pos;
    size_t next_nalu_pos = nalu_pos;
    int found_annex = found_nalu_annex(data, data_size, pos, next_nalu_pos);
    if(!found_annex){
        return -2;
    }
    
    start_pos = pos;
    nalu_pos = next_nalu_pos;
    pos = next_nalu_pos;
    found_annex = found_nalu_annex(data, data_size, pos, next_nalu_pos);
    length = pos - nalu_pos;
    
    pos = start_pos;
    return 0;
}

static
void dump_video_buf(uint8_t *raw_video_data, size_t raw_video_size){
    size_t raw_video_pos = 0;
    size_t raw_video_len = 0;
    int ret = 0;
    while(1){
        size_t nalu_pos = raw_video_pos;
        ret = read_h264_next_nalu(raw_video_data, raw_video_size, raw_video_pos, raw_video_len, nalu_pos);
        if(ret < 0){
            break;
        }
        dbgi("nalu: pos=%ld, nalu=%ld, len=%ld, end=%ld", raw_video_pos, nalu_pos, raw_video_len, nalu_pos+raw_video_len);
    }
}

static
int load_raw_file(const char * fname, bool dump, uint8_t **buf, size_t * buf_size){
    FILE * fp_rtmp_video = NULL;
    uint8_t * raw_video_data = NULL;
    size_t raw_video_size = 0;
    
    int ret = 0;
    do{
        fp_rtmp_video = fopen(fname, "rb");
        if(!fp_rtmp_video){
            dbge("fail to open video file [%s]", fname);
            break;
        }
        ret = fseek(fp_rtmp_video, 0, SEEK_END);
        raw_video_size = ftell(fp_rtmp_video);
        ret = fseek(fp_rtmp_video, 0, SEEK_SET);
        dbgi("video file size %ld", raw_video_size);
        if(raw_video_size <= 0){
            dbge("invalid video file size");
            ret = -1;
            break;
        }
        raw_video_data = (uint8_t *) malloc(raw_video_size);
        long bytes = fread(raw_video_data, 1, raw_video_size, fp_rtmp_video);
        if(bytes != raw_video_size){
            dbge("read video file fail, %ld", bytes);
            ret = -1;
            break;
        }
        
        if(dump){
            dump_video_buf(raw_video_data, raw_video_size);
        }
        
        if(buf){
            *buf = raw_video_data;
            raw_video_data = NULL;
        }
        if(buf_size){
            *buf_size = raw_video_size;
            raw_video_size = 0;
        }
        ret = 0;
        
    }while(0);
    if(fp_rtmp_video){
        fclose(fp_rtmp_video);
        fp_rtmp_video = NULL;
    }
    if(raw_video_data){
        free(raw_video_data);
        raw_video_data = NULL;
    }
    return ret;
}

static
int64_t get_now_ms(){
    unsigned long milliseconds_since_epoch =
    std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count();
    return milliseconds_since_epoch;
};

static
int read_audio_frame(char* data, int size, char** pp, char** frame, int* frame_size)
{
    char* p = *pp;
    
    if (!srs_aac_is_adts(p, (int)(size - (p - data))) ) {
        srs_human_trace("aac adts raw data invalid.");
        return -1;
    }
    
    // @see srs_audio_write_raw_frame
    // each frame prefixed aac adts header, '1111 1111 1111'B, that is 0xFFF.,
    // for instance, frame = FF F1 5C 80 13 A0 FC 00 D0 33 83 E8 5B
    *frame = p;
    // skip some data.
    // @remark, user donot need to do this.
    p += srs_aac_adts_frame_size(p, (int)(size - (p - data)) );
    
    *pp = p;
    *frame_size = (int)(p - *frame);
    if (*frame_size <= 0) {
        srs_human_trace("aac adts raw data invalid.");
        return -1;
    }
    
    return 0;
}


class H264FileReader{
    uint8_t * buf_ = NULL;
    size_t buf_size_;
    size_t next_read_pos_ = 0;
    size_t next_data_pos_ = 0;
    size_t next_data_len_ = 0;
    int64_t interval_ms_ = 1000/25;
    int64_t next_time_ms_ = 0;
    int frameNum_ = 0;
public:
    H264FileReader(){
        
    }
    virtual ~H264FileReader(){
        close();
    }
    int open(const char * filename, int fps){
        int ret = 0;
        do{
            close();
            ret = load_raw_file(filename, false, &buf_, &buf_size_);
            if(ret){
                break;
            }
            ret = next();
            if(ret){
                break;
            }
            interval_ms_ = 1000/fps;
            next_time_ms_ = 0;
            ret = 0;
        }while(0);
        if(ret){
            close();
        }
        return ret;
    }
    void close(){
        if(buf_){
            free(buf_);
            buf_ = NULL;
        }
        rewind();
        frameNum_ = 0;
    }
    int rewind(){
        next_read_pos_ = 0;
        next_data_pos_ = 0;
        next_data_len_ = 0;
        if(buf_){
            return next();
        }else{
            return -1;
        }
    }
    int next(){
        int ret = read_h264_next_nalu(buf_, buf_size_, next_read_pos_, next_data_len_, next_data_pos_);
        if(ret){
            return ret;
        }
        ++frameNum_;
        next_time_ms_ += interval_ms_;
        uint8_t nut = payload()[0]& 0x1f;
        if((nut != 7) && (nut != 8)){
//            next_time_ms_ += interval_ms_;
        }
        return ret;
    }
    int frameNum(){
        return frameNum_;
    }
    uint8_t * data(){
        return &buf_[next_read_pos_];
    }
    size_t dataLength(){
        return next_data_pos_ - next_read_pos_ + next_data_len_;
    }
    uint8_t * payload(){
        return &buf_[next_data_pos_];
    }
    size_t payloadLength(){
        return next_data_len_;
    }
    int64_t timestamp(){
        return next_time_ms_;
    }
};

class AACFileReader{
    uint8_t * buf_ = NULL;
    size_t buf_size_;
    char * next_p_ = 0;
    uint8_t * next_data_ = 0;
    size_t next_data_len_ = 0;
    int64_t frame_duration_ = 40;
    int64_t next_time_ms_ = 0;
    int frameNum_ = 0;
public:
    AACFileReader(){
        
    }
    virtual ~AACFileReader(){
        close();
    }
    int open(const char * filename, int64_t frame_duration){
        int ret = 0;
        do{
            close();
            ret = load_raw_file(filename, false, &buf_, &buf_size_);
            if(ret){
                break;
            }
            frame_duration_ = frame_duration;
            next_p_ = (char*)buf_;
            ret = next();
            if(ret){
                break;
            }
            next_time_ms_ = 0;
            ret = 0;
        }while(0);
        if(ret){
            close();
        }
        return ret;
    }
    void close(){
        rewind();
        if(buf_){
            free(buf_);
            buf_ = NULL;
        }
        frameNum_ = 0;
    }
    int rewind(){
        next_p_ = (char*)buf_;
        next_data_ = buf_;
        next_data_len_ = 0;
        if(buf_){
            return next();
        }else{
            return -1;
        }
    }
    int next(){
        char * data = NULL;
        int sz = 0;
        int ret = read_audio_frame((char*)buf_, (int)buf_size_, &next_p_, &data, &sz);
        if(ret){
            return ret;
        }
        next_data_ = (uint8_t*)data;
        next_data_len_ = sz;
        next_time_ms_ += frame_duration_;
        ++frameNum_;
        return ret;
    }
    int frameNum(){
        return frameNum_;
    }
    uint8_t * data(){
        return next_data_;
    }
    size_t dataLength(){
        return next_data_len_;
    }
    uint8_t * payload(){
        return data();
    }
    size_t payloadLength(){
        return dataLength();
    }
    int64_t timestamp(){
        return next_time_ms_;
    }
};

static
int push_video_frame(srs_rtmp_t rtmp, H264FileReader * video){
    int ret = 0;
    do{
        ret = srs_h264_write_raw_frames(rtmp, (char*)video->data(), (int)video->dataLength(), (int)video->timestamp(), (int)video->timestamp());
        if (ret != 0) {
            const char * errmsg = "";
            if (srs_h264_is_dvbsp_error(ret)) {
                errmsg = "ignore drop video";
            } else if (srs_h264_is_duplicated_sps_error(ret)) {
                errmsg = "ignore dup sps";
            } else if (srs_h264_is_duplicated_pps_error(ret)) {
                errmsg = "ignore dup pps";
            } else {
                srs_human_trace("|No.%04d| video fail with known ret=%d", video->frameNum(), ret);
                break;
            }
            srs_human_trace("|No.%04d| video warn=[%s], ret=%d", video->frameNum(), errmsg,  ret);
        }else{
            // 5bits, 7.3.1 NAL unit syntax,
            // ISO_IEC_14496-10-AVC-2003.pdf, page 44.
            //  7: SPS, 8: PPS, 5: I Frame, 1: P Frame, 9: AUD, 6: SEI
            uint8_t nut = video->payload()[0] & 0x1f;
            const char * nutname=(nut == 7? "SPS":(nut == 8? "PPS":(nut == 5? "I":(nut == 1? "P":(nut == 9? "AUD":(nut == 6? "SEI":"Unknown"))))));
            
            srs_human_trace("|No.%04d| sent video, ts=%lld, sz=%ld, h264=[%s]", video->frameNum(), video->timestamp(), video->dataLength(), nutname);
        }
        ret = video->next();
        if(ret){
            ret = video->rewind();
        }
    }while(0);
    return ret;
}

static
int push_audio_frame(srs_rtmp_t rtmp, AACFileReader * audio){
    int ret = 0;
    do {
        // 0 = Linear PCM, platform endian
        // 1 = ADPCM
        // 2 = MP3
        // 7 = G.711 A-law logarithmic PCM
        // 8 = G.711 mu-law logarithmic PCM
        // 10 = AAC
        // 11 = Speex
        char sound_format = 10;
        // 2 = 22 kHz
        char sound_rate = 2;
        // 1 = 16-bit samples
        char sound_size = 1;
        // 1 = Stereo sound
        char sound_type = 1;
        
        ret = srs_audio_write_raw_frame(rtmp,
                                        sound_format, sound_rate, sound_size, sound_type,
                                        (char*)audio->data(), (int)audio->dataLength(), (uint32_t)audio->timestamp());
        if (ret) {
            srs_human_trace("|No.%04d| send audio fail, ret=%d", audio->frameNum(),  ret);
            ret = -1;
            break;
        }
        srs_human_trace("|No.%04d| sent audio, ts=%lld, sz=%ld", audio->frameNum()
                        , audio->timestamp()
                        , audio->dataLength());
        ret = audio->next();
        if(ret){
            ret = audio->rewind();
        }
    } while (0);
    return ret;
}

int main_push_av(int argc, char** argv)
{
    const char* video_raw_file = NULL;
    const char* audio_raw_file = NULL;
    const char* rtmp_url = NULL;
    
    rtmp_url = "rtmp://127.0.0.1/myapp/raw";
    video_raw_file = "/Users/simon/Downloads/720p.h264.raw";
    audio_raw_file = "/Users/simon/Downloads/audio.raw.aac";
    
    dbgi("video_raw_file=[%s]", video_raw_file);
    dbgi("audio_raw_file=[%s]", audio_raw_file);
    dbgi("rtmp_url=[%s]", rtmp_url);
    
    srs_rtmp_t rtmp = 0;
    H264FileReader * video = NULL;
    AACFileReader * audio = NULL;
    int ret = 0;
    do{
        if(video_raw_file){
            video = new H264FileReader();
            ret = video->open(video_raw_file, 25);
            if(ret){
                break;
            }
        }
        if(audio_raw_file){
            audio = new AACFileReader();
            ret = audio->open(audio_raw_file, 45);
            if(ret){
                break;
            }
        }
        
        // connect rtmp context
        rtmp = srs_rtmp_create(rtmp_url);
        
        if (srs_rtmp_handshake(rtmp) != 0) {
            dbge("simple handshake failed.");
            ret = -1;
            break;
        }
        dbgi("simple handshake success");
        
        if (srs_rtmp_connect_app(rtmp) != 0) {
            dbge("connect vhost/app failed.");
            ret = -1;
            break;
        }
        dbgi("connect vhost/app success");
        
        if (srs_rtmp_publish_stream(rtmp) != 0) {
            dbge("publish stream failed.");
            ret = -1;
            break;
        }
        dbgi("publish stream success");
        const int64_t MIN_DIFF = 0;
        int64_t start_time_ms = get_now_ms();
        while(1){
            int64_t elapsed_ms = get_now_ms() - start_time_ms;
            if(video && (video->timestamp() - elapsed_ms) <= MIN_DIFF){
//            if(video){
                ret = push_video_frame(rtmp, video);
                if(ret) break;
            }
            if(audio && (audio->timestamp() - elapsed_ms) <= MIN_DIFF){
                ret = push_audio_frame(rtmp, audio);
                if(ret) break;
            }
            // get min timestamp of next
            int64_t next_ts = 0x00FFFFFFFFFFFFFF;
            if(video && video->timestamp() <= next_ts){
                next_ts = video->timestamp();
            }
            if(audio && audio->timestamp() <= next_ts){
                next_ts = audio->timestamp();
            }
            int64_t ms = start_time_ms + next_ts - get_now_ms();
            if(ms > 0){
//                dbgi("next_ts=%lld(+%lld)", next_ts, ms);
                usleep((unsigned)(1000*ms));
            }
            
            ret = 0;
        }// loop
        
    }while (0);
    
    if(rtmp){
        srs_rtmp_destroy(rtmp);
        rtmp = 0;
    }
    
    if(video){
        delete video;
        video = NULL;
    }
    
    if(audio){
        delete audio;
        audio = NULL;
    }
    
    return 0;
}

#include <thread>
#include "FFMpegFramework.hpp"
#include "NUtil.hpp"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
//#include "rav_record.h"

#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)

#define NERROR_FMT_SET(code, ...)\
    NThreadError::setError(code, fmt::format(__VA_ARGS__))
class NThreadError{
public:
    static const std::string& lastMsg() {
        return msg_;
    }
    
    static int lastCode() {
        return code_;
    }
    
    static void setError(int code, const std::string& msg){
        code_ = code;
        msg_ = msg;
    }
    
private:
    static thread_local int code_ ;
    static thread_local std::string msg_;
};
thread_local int NThreadError::code_ = 0;
thread_local std::string NThreadError::msg_ = "";

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
    
    int64_t convert(int64_t pts){
        pts = pts * dst_.den/dst_.num/src_.den/src_.num;
        return pts;
    }
private:
    AVRational src_;
    AVRational dst_;
};

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
};

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
                
                dbge("Error opening file for output, %d\n", err);
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
                dbge("Error new video stream\n");
                ret = -1;
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
                dbge("Error new audio stream\n");
                ret = -1;
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

typedef std::function<int(const AVFrame * )> FFFrameFunc;
typedef std::function<int(AVPacket * pkt)> FFPacketFunc;

struct FFImageConfig{
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    int width = -1;
    int height = -1;
    
    void assign(const AVFrame* frame){
        pix_fmt = (AVPixelFormat)frame->format;
        width = frame->width;
        height = frame->height;
    }
    
    bool valid()const{
        return !(pix_fmt <= AV_PIX_FMT_NONE || width <= 0 || height <= 0);
    }
    
    bool equal(const FFImageConfig& other)const{
        if(&other == this) return true;
        return (pix_fmt == other.pix_fmt && width == other.width && height == other.height);
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
        frame_->format = dstcfg_.pix_fmt;
        frame_->width = dstcfg_.width;
        frame_->height = dstcfg_.height;
        
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
        swsctx_ = sws_getContext(srccfg.width, srccfg.height, srccfg.pix_fmt
                                 , dstcfg.width, dstcfg.height, dstcfg.pix_fmt
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

class FFVideoEncoder{
public:
//    /static const AVCodecID DEFAULT_CODEC_ID = AV_CODEC_ID_H264;
    static const AVPixelFormat DEFAULT_PIX_FMT = AV_PIX_FMT_YUV420P;
    static const int DEFAULT_WIDTH = 640;
    static const int DEFAULT_HEIGHT = 480;
    static const int DEFAULT_FRAMERATE = 25;
    static const int DEFAULT_GOPSIZE = DEFAULT_FRAMERATE;
    static const int64_t DEFAULT_BITRATE = 500000;
    
public:
    FFVideoEncoder():timebase_({1, 1000}){
        srcImgCfg_.pix_fmt = DEFAULT_PIX_FMT;
        srcImgCfg_.width = DEFAULT_WIDTH;
        srcImgCfg_.height = DEFAULT_HEIGHT;
    }
    
    virtual ~FFVideoEncoder(){
        close();
    }
    
    void setCodecName(const std::string& codec_name){
        codec_ = avcodec_find_encoder_by_name(codec_name.c_str());
        if(codec_){
            codecId_ = codec_->id;
        }else{
            codecId_ = AV_CODEC_ID_NONE;
        }
    }
    
    void setCodecId(AVCodecID codec_id){
        codecId_ = codec_id;
    }
    
    void setFramerate(int framerate){
        framerate_ = framerate;
    }
    
    void setBitrate(int64_t bitrate){
        bitrate_ = bitrate;
    }
    
    void setGopSize(int gop_size){
        gopSize_ = gop_size;
    }
    
    void setSrcImageConfig(const FFImageConfig& cfg){
        srcImgCfg_ = cfg;
    }
    
    void setTimebase(const AVRational& timebase){
        timebase_ = timebase;
    }
    
    void setCodecOpt(const std::string& k, const std::string& v){
        codecOpts_[k] = v;
    }
    
    AVCodecID codecId()const{
        return codecId_;
    }
    
    const FFImageConfig& encodeImageCfg() const{
        return encImgCfg_;
    }
    
    int open(){
        int ret = 0;
        do{
            
            AVCodecID codec_id = (AVCodecID)codecId_;
            if(codec_id <= AV_CODEC_ID_NONE){
                ret = -1;
                NERROR_FMT_SET(ret, "NOT spec video codec");
                break;
            }
            
            const AVCodec * codec = codec_;
            if(!codec){
                codec = avcodec_find_encoder(codec_id);
                if (!codec) {
                    ret = -1;
                    NERROR_FMT_SET(ret, "fail to avcodec_find_encoder video codec id [{}]",
                                   (int)codec_id);
                    break;
                }
            }
            
            ctx_ = avcodec_alloc_context3(codec);
            if (!ctx_) {
                ret = -1;
                NERROR_FMT_SET(ret, "fail to avcodec_alloc_context3 video codec [{}]-[{}]",
                                     (int)codec->id, codec->name);
                break;
            }
            
            pkt_ = av_packet_alloc();
            if (!pkt_){
                ret = -1;
                NERROR_FMT_SET(ret, "can't allocate video packet");
                break;
            }
            
            ctx_->bit_rate = bitrate_ > 0 ? bitrate_ : DEFAULT_BITRATE;
            
            // TODO:
            ctx_->max_b_frames = 0; // aaa
            ctx_->flags |= CODEC_FLAG_QSCALE;
            ctx_->rc_min_rate = ctx_->bit_rate*2/3;
            ctx_->rc_max_rate = ctx_->bit_rate *2;
            ctx_->rc_buffer_size = (int)(ctx_->rc_max_rate * 2);
            
            ctx_->width = srcImgCfg_.width > 0 ? srcImgCfg_.width : DEFAULT_WIDTH;
            ctx_->height = srcImgCfg_.height > 0 ? srcImgCfg_.height : DEFAULT_HEIGHT;
            
            ctx_->time_base = timebase_;
            
            ctx_->framerate = (AVRational){framerate_ > 0? framerate_ : DEFAULT_FRAMERATE, 1};
            
            
            /* emit one intra frame every ten frames
             * check frame pict_type before passing frame
             * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
             * then gop_size is ignored and the output of encoder
             * will always be I frame irrespective to gop_size
             */
            ctx_->gop_size = gopSize_ > 0 ? gopSize_ : DEFAULT_GOPSIZE;
            
            
            ctx_->pix_fmt = selectPixelFormat(codec, srcImgCfg_.pix_fmt);
            for(auto& it : codecOpts_){
                av_opt_set(ctx_->priv_data, it.first.c_str(), it.second.c_str(), 0);
            }
            
//            if (codec->id == AV_CODEC_ID_H264){
//                //av_opt_set(ctx_->priv_data, "preset", "slow", 0);
//                av_opt_set(ctx_->priv_data, "preset", "fast", 0);
//                //av_opt_set(ctx_->priv_data, "preset", "fast", AV_OPT_SEARCH_CHILDREN);
//                /*
//                ultrafast
//                superfast
//                veryfast
//                faster
//                fast
//                medium – default preset
//                slow
//                slower
//                veryslow
//                 */
//            }
            
            ret = avcodec_open2(ctx_, codec, NULL);
            if (ret != 0) {
                NERROR_FMT_SET(ret, "fail to avcodec_open2 video codec [{}]-[{}], err=[{}]",
                                     (int)codec->id, codec->name, av_err2str(ret));
                break;
            }
            
            encImgCfg_.pix_fmt = ctx_->pix_fmt;
            encImgCfg_.width = ctx_->width;
            encImgCfg_.height = ctx_->height;
            srcImgCfg_.match(encImgCfg_);
            ret = adatper_.open(srcImgCfg_, encImgCfg_);
            if(ret != 0){
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
        if(ctx_){
            avcodec_free_context(&ctx_);
            ctx_ = nullptr;
        }
        
        if(pkt_){
            av_packet_free(&pkt_);
            pkt_ = nullptr;
        }
        adatper_.close();
        codecOpts_.clear();
    }
    
    int encode(const AVFrame *srcframe, const FFPacketFunc& func){
        int ret = 0;
        if(srcframe){
            ret = adatper_.adapt(srcframe, [this, &func](const AVFrame * dstframe)->int{
                return doEncode(dstframe, func);
            });
        }else{
            ret = doEncode(nullptr, func);
        }

        return ret;
    }

private:
    int doEncode(const AVFrame *frame, const FFPacketFunc& func){
        int ret = 0;
        ret = avcodec_send_frame(ctx_, frame);
        if (ret != 0) {
            NERROR_FMT_SET(ret, "fail to avcodec_send_frame video, err=[{}]", av_err2str(ret));
            return ret;
        }
        
        while (ret >= 0) {
            ret = avcodec_receive_packet(ctx_, pkt_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
                return 0;
            }else if (ret != 0) {
                NERROR_FMT_SET(ret, "fail to avcodec_receive_packet video, err=[{}]", av_err2str(ret));
                return ret;
            }
            ret = func(pkt_);
            av_packet_unref(pkt_);
        }
        return ret;
    }
    
    static AVPixelFormat selectPixelFormat(const AVCodec *codec, AVPixelFormat prefer_fmt){
        if(!codec->pix_fmts || *codec->pix_fmts <= AV_PIX_FMT_NONE){
            // no supported format
            if(prefer_fmt > AV_PIX_FMT_NONE){
                // select prefer format
                return prefer_fmt;
            }else{
                // select default format
                return DEFAULT_PIX_FMT;
            }
        }
        
        const enum AVPixelFormat *p = codec->pix_fmts;
        while (*p != AV_PIX_FMT_NONE) {
            if (*p == prefer_fmt){
                // match, select it
                return prefer_fmt;
            }
            p++;
        }
        // NOT match, select first supported format
        return *codec->pix_fmts;
    }
    
private:
    AVCodecID codecId_ = AV_CODEC_ID_NONE;
    const AVCodec * codec_ = nullptr;
    int framerate_ = -1;
    int gopSize_ = -1;
    int64_t bitrate_ = -1;
    AVCodecContext * ctx_ = nullptr;
    AVPacket * pkt_ = nullptr;
    FFImageAdapter adatper_;
    FFImageConfig srcImgCfg_;
    FFImageConfig encImgCfg_;
    AVRational timebase_;
    std::map<std::string, std::string> codecOpts_;
};


struct FFSampleConfig{
    int samplerate ;
    uint64_t channellayout ;
    AVSampleFormat samplefmt;
    int framesize;
    
    FFSampleConfig(){
        reset();
    }
    
    void reset(){
        samplerate = -1;
        channellayout = 0;
        samplefmt = AV_SAMPLE_FMT_NONE;
        framesize = 0;
    }
    
    void assign(const AVFrame* frame){
        samplerate = frame->sample_rate;
        channellayout = frame->channel_layout;
        samplefmt = (AVSampleFormat)frame->format;
    }
    
    bool valid()const{
        return (samplerate > 0 && channellayout > 0 && samplefmt > AV_SAMPLE_FMT_NONE);
    }
    
    bool equalBase(const FFSampleConfig& other)const{
        return (samplerate == other.samplerate
                && channellayout == other.channellayout
                && samplefmt == other.samplefmt);
    }
    
    bool equalBase(const AVFrame* frame)const{
        return (samplerate == frame->sample_rate
                && channellayout == frame->channel_layout
                && (int)samplefmt == frame->format);
    }
    
    bool equal(const FFSampleConfig& other)const{
        return (equalBase(other)
                && framesize == other.framesize);
    }
    
    void match(const FFSampleConfig& stdcfg){
        FFSampleConfig& cfg = *this;
        if(cfg.samplefmt <= AV_SAMPLE_FMT_NONE){
            cfg.samplefmt = stdcfg.samplefmt;
        }
        if(cfg.samplerate <= 0){
            cfg.samplerate = stdcfg.samplerate;
        }
        if(cfg.channellayout == 0){
            cfg.channellayout = stdcfg.channellayout;
        }
        if(cfg.framesize <= 0 && stdcfg.framesize > 0){
            cfg.framesize = stdcfg.framesize;
        }
    }
};

class FFFramesizeConverter{
public:
    FFFramesizeConverter():outTimebase_({1, 1000}){
        
    }
    
    virtual ~FFFramesizeConverter(){
        close();
    }
    
    int open(const AVRational& timebase, int framesize){
        close();
        outTimebase_ = timebase;
        dstCfg_.framesize = framesize;
        return 0;
    }
    
    void close(){
        if(outFrame_){
            av_frame_free(&outFrame_);
            outFrame_ = nullptr;
        }
        dstCfg_.reset();
    }
    
    int convert(const AVFrame * srcframe, const FFFrameFunc& func){
        int ret = 0;
        if(!srcframe){
            if(outFrame_ && outFrame_->nb_samples > 0){
                // fflush remain samples
                ret = func(outFrame_);
            }
            return ret;
        }
        
        if(!outFrame_){
            if(srcframe->nb_samples == dstCfg_.framesize || dstCfg_.framesize <= 0){
                // same frame size, output directly
                ret = func(srcframe);
                return ret;
            }
            
            dstCfg_.assign(srcframe);
            outFrame_ = av_frame_alloc();
        }
        
        outFrame_->pts = srcframe->pts - samples2Duration(outFrame_->nb_samples);
        
        int dst_offset = outFrame_->nb_samples;
        int src_offset = 0;
        while(src_offset < srcframe->nb_samples){
            checkAssignFrame();
            int remain_samples = srcframe->nb_samples-src_offset;
            int min = std::min(remain_samples+dst_offset, dstCfg_.framesize);
            int num_copy = min - dst_offset;
            
            av_samples_copy(outFrame_->data, srcframe->data,
                            dst_offset,
                            src_offset,
                            num_copy, outFrame_->channels, (AVSampleFormat)outFrame_->format);
            dst_offset += num_copy;
            src_offset += num_copy;
            outFrame_->nb_samples = dst_offset;
            if(outFrame_->nb_samples == dstCfg_.framesize){
                outFrame_->pkt_size = av_samples_get_buffer_size(outFrame_->linesize, outFrame_->channels,
                                                                 outFrame_->nb_samples, (AVSampleFormat)outFrame_->format,
                                                                 1);
                ret = func(outFrame_);
                av_frame_unref(outFrame_);
                if(ret != 0){
                    return ret;
                }
                outFrame_->pts = outFrame_->pts + samples2Duration(outFrame_->nb_samples);
                outFrame_->nb_samples = 0;
                dst_offset = 0;
            }
        }
        return 0;
    }
    
private:
    int64_t samples2Duration(int out_samples){
        int64_t duration = out_samples * outTimebase_.den / dstCfg_.samplerate / outTimebase_.num;
        return duration;
    }
    
    void checkAssignFrame(){
        if(!outFrame_->buf[0]){
            assignFrame();
            outFrame_->nb_samples = dstCfg_.framesize;
            if(outFrame_->nb_samples > 0){
                av_frame_get_buffer(outFrame_, 0);
            }
            outFrame_->nb_samples = 0;
        }
    }
    
    void assignFrame(){
        outFrame_->sample_rate = dstCfg_.samplerate;
        outFrame_->format = dstCfg_.samplefmt;
        outFrame_->channel_layout = dstCfg_.channellayout;
        outFrame_->channels = av_get_channel_layout_nb_channels(outFrame_->channel_layout);
    }
    
private:
    //int framesize_ = 0;
    AVRational outTimebase_;
    AVFrame * outFrame_ = nullptr;
    FFSampleConfig dstCfg_;
};




class FFSampleConverter{
public:
    FFSampleConverter():timebase_({1, 1000}){
        
    }
    
    virtual ~FFSampleConverter(){
        close();
    }
    
    int open(const FFSampleConfig& srccfg, const FFSampleConfig& dstcfg, const AVRational& timebase){
        close();
        
        srccfg_ = srccfg;
        dstcfg_ = dstcfg;
        timebase_ = timebase;
        
        int ret = 0;
        if(!srccfg.equalBase(dstcfg)){
            
            swrctx_ = swr_alloc_set_opts(
                                        NULL,
                                        dstcfg.channellayout,
                                        dstcfg.samplefmt,
                                        dstcfg.samplerate,
                                        srccfg.channellayout,
                                        srccfg.samplefmt,
                                        srccfg.samplerate,
                                        0, NULL);
            if(!swrctx_){
                NERROR_FMT_SET(ret, "fail to swr_alloc_set_opts");
                return -61;
            }
            
            ret = swr_init(swrctx_);
            if(ret != 0){
                NERROR_FMT_SET(ret, "fail to swr_init , err=[{}]", av_err2str(ret));
                return ret;
            }
            
            // alloc buffer
            //reallocBuffer(dstcfg_.framesize);
            
            frame_ = av_frame_alloc();
            
        }else{
            ret = sizeConverter_.open(timebase_, dstcfg_.framesize);
        }
        return ret;
    }
    
    void close(){
        if(swrctx_){
            if(swr_is_initialized(swrctx_)){
                swr_close(swrctx_);
            }
            swr_free(&swrctx_);
        }
        
        if(frame_){
            av_frame_free(&frame_);
            frame_ = nullptr;
        }
        
        sizeConverter_.close();
        realDstFramesize_ = 0;
    }
    
    int convert(const AVFrame * srcframe, const FFFrameFunc& func){
        if(!swrctx_){
            return sizeConverter_.convert(srcframe, func);
        }
        int ret = 0;
        if(!srcframe){
            if(remainDstSamples_ > 0){
                // pull remain samples
                checkAssignFrame();
                frame_->pts = nextDstPTS_;
                ret = swr_convert_frame(swrctx_, frame_, NULL);
                if(ret != 0){
                    av_frame_unref(frame_);
                    NERROR_FMT_SET(ret, "fail to swr_convert_frame 1st, err=[{}]", av_err2str(ret));
                    return ret;
                }
                frame_->pkt_size = av_samples_get_buffer_size(frame_->linesize, frame_->channels,
                                                              frame_->nb_samples, (AVSampleFormat)frame_->format, 1);
                if(frame_->nb_samples > 0){
                    ret = func(frame_);
                }
                av_frame_unref(frame_);
            }
            return ret;
        }
        
        int out_samples = swr_get_out_samples(swrctx_, srcframe->nb_samples);
        if(out_samples < realDstFramesize_){
            // not enough output samples
            ret = swr_convert_frame(swrctx_, NULL, srcframe);
            if(ret != 0){
                NERROR_FMT_SET(ret, "fail to swr_convert_frame 2nd, err=[{}]", av_err2str(ret));
                return ret;
            }
            return 0;
        }
        
//        if(dstcfg_.framesize <= 0){
//            reallocBuffer(srcframe->nb_samples);
//        }
        
        checkAssignFrame();

        int64_t dst_pts = srcframe->pts;
        frame_->pts = dst_pts - dstDuration(remainDstSamples_);
        frame_->pkt_size = av_samples_get_buffer_size(frame_->linesize, frame_->channels,
                                                      frame_->nb_samples, (AVSampleFormat)frame_->format, 1);
        
        ret = swr_convert_frame(swrctx_, frame_, srcframe);
        if(ret != 0){
            av_frame_unref(frame_);
            NERROR_FMT_SET(ret, "fail to swr_convert_frame 3rd, err=[{}]", av_err2str(ret));
            return ret;
        }
        
        // TODO:
//        static Int64Relative srcRelpts_;
//        const AVFrame * frame = frame_;
//        odbgd("src frame0: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
//              , frame->pts, srcRelpts_.offset(frame->pts), srcRelpts_.delta(frame->pts)
//              , frame->channels, frame->nb_samples, frame->pkt_size
//              , av_get_sample_fmt_name((AVSampleFormat)frame->format)
//              , frame->sample_rate);
        
        ret = func(frame_);
        av_frame_unref(frame_);
        if(ret){
            return ret;
        }
        
        out_samples -= frame_->nb_samples;
        frame_->pts += dstDuration(frame_->nb_samples);
        nextDstPTS_ = frame_->pts;
        
        while(realDstFramesize_>0 && out_samples >= realDstFramesize_){
            checkAssignFrame();
            //frame_->pts = nextPTS(srcframe->pts, out_samples);
            ret = swr_convert_frame(swrctx_, frame_, NULL);
            if(ret != 0){
                av_frame_unref(frame_);
                NERROR_FMT_SET(ret, "fail to swr_convert_frame 4th, err=[{}]", av_err2str(ret));
                return ret;
            }
            frame_->pkt_size = av_samples_get_buffer_size(frame_->linesize, frame_->channels,
                                                          frame_->nb_samples, (AVSampleFormat)frame_->format, 1);
            
//            // TODO:
//            const AVFrame * frame = frame_;
//            odbgd("src frame1: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
//                  , frame->pts, srcRelpts_.offset(frame->pts), srcRelpts_.delta(frame->pts)
//                  , frame->channels, frame->nb_samples, frame->pkt_size
//                  , av_get_sample_fmt_name((AVSampleFormat)frame->format)
//                  , frame->sample_rate);
            
            ret = func(frame_);
            av_frame_unref(frame_);
            if(ret){
                return ret;
            }
            
            out_samples -= frame_->nb_samples;
            frame_->pts += dstDuration(frame_->nb_samples);
            nextDstPTS_ = frame_->pts;
            //out_samples = swr_get_out_samples(swrctx_, 0);
        }
        
        remainDstSamples_ = out_samples;
        
        return 0;
    }
    
private:
    int64_t dstDuration(int out_samples){
        int64_t duration = out_samples * timebase_.den / dstcfg_.samplerate / timebase_.num;
        return duration;
    }
    
    int checkAssignFrame(){
        frame_->sample_rate = dstcfg_.samplerate;
        frame_->format = dstcfg_.samplefmt;
        frame_->channel_layout = dstcfg_.channellayout;
        frame_->channels = av_get_channel_layout_nb_channels(frame_->channel_layout);
        
        int ret = 0;
        if(dstcfg_.framesize > 0){
            frame_->nb_samples = dstcfg_.framesize;
            ret = av_frame_get_buffer(frame_, 0);
        }
        return ret;
    }
    
//    int reallocBuffer(int frameSize){
//        // alloc buffer
//        int ret = 0;
//        if(frameSize > 0 && frameSize > realDstFramesize_){
//            if(!frame_){
//                frame_ = av_frame_alloc();
//                frame_->sample_rate = dstcfg_.samplerate;
//                frame_->format = dstcfg_.samplefmt;
//                frame_->channel_layout = dstcfg_.channellayout;
//                frame_->channels = av_get_channel_layout_nb_channels(frame_->channel_layout);
//            }
//
//            if(frame_ && realDstFramesize_ > 0){
//                av_frame_unref(frame_);
//                realDstFramesize_ = 0;
//            }
//
//            frame_->nb_samples = frameSize;
//            ret = av_frame_get_buffer(frame_, 0);
//            if(ret == 0){
//                realDstFramesize_ = frameSize;
//            }
//        }
//        return ret;
//    }
    
private:
    FFSampleConfig srccfg_;
    FFSampleConfig dstcfg_;
    AVRational timebase_;
    SwrContext * swrctx_ = nullptr;
    AVFrame * frame_ = nullptr;
    int realDstFramesize_ = -1;
    int64_t nextDstPTS_ = 0;
    int remainDstSamples_ = 0;
    FFFramesizeConverter sizeConverter_;
};

class FFAudioEncoder{
public:
    //static const AVCodecID DEFAULT_CODEC_ID = AV_CODEC_ID_AAC;
    //static const int DEFAULT_CLOCKRATE = 44100;
    //static const int DEFAULT_CHANNELS = 2;
    static const int64_t DEFAULT_BITRATE = 24000;
    
public:
    FFAudioEncoder():timebase_((AVRational){1, 1000}){
        
    }
    
    virtual ~FFAudioEncoder(){
        close();
    }
    
    void setCodecName(const std::string& codec_name){
        codec_ = avcodec_find_encoder_by_name(codec_name.c_str());
        if(codec_){
            codecId_ = codec_->id;
        }else{
            codecId_ = AV_CODEC_ID_NONE;
        }
    }
    
    void setCodecId(AVCodecID codec_id){
        codecId_ = codec_id;
        codec_ = avcodec_find_encoder(codec_id);
    }
    
    void setBitrate(int64_t bitrate){
        bitrate_ = bitrate;
    }
    
    void setSrcConfig(const FFSampleConfig& cfg){
        srcCfg_ = cfg;
    }
    
    void setTimebase(const AVRational& timebase){
        timebase_ = timebase;
    }
    
    const FFSampleConfig& srcConfig(){
        return srcCfg_;
    }
    
    const FFSampleConfig& encodeConfig(){
        return encodeCfg_;
    }
    
    AVCodecID codecId() const{
        return codecId_;
    }
    
    int open(){
        int ret = 0;
        do{
            AVCodecID codec_id = (AVCodecID)codecId_;
            if(codec_id <= AV_CODEC_ID_NONE){
                ret = -1;
                break;
            }
            
            const AVCodec * codec = codec_;
            if(!codec){
                codec = avcodec_find_encoder(codec_id);
                if (!codec) {
                    ret = -1;
                    NERROR_FMT_SET(ret, "fail to avcodec_find_encoder audio codec id [{}]",
                                         (int)codec_id);
                    break;
                }
            }
            
            ctx_ = avcodec_alloc_context3(codec);
            if (!ctx_) {
                ret = -1;
                NERROR_FMT_SET(ret, "fail to avcodec_alloc_context3 audio codec [{}][{}]",
                                     (int)codec->id, codec->name);
                break;
            }
            
            pkt_ = av_packet_alloc();
            if (!pkt_){
                NERROR_FMT_SET(ret, "fail to av_packet_alloc audio");
                ret = -1;
                break;
            }
            
            ctx_->bit_rate = bitrate_ > 0 ? bitrate_ : DEFAULT_BITRATE;
            ctx_->sample_fmt     = selectSampleFormat(codec, srcCfg_.samplefmt);
            ctx_->sample_rate    = selectSampleRate(codec, srcCfg_.samplerate);
            ctx_->channel_layout = selectChannelLayout(codec, srcCfg_.channellayout );
            ctx_->channels       = av_get_channel_layout_nb_channels(ctx_->channel_layout);
            ctx_->time_base      = timebase_;
            //ctx_->time_base      = srcCfg_.timebase;
            
            ret = avcodec_open2(ctx_, codec, NULL);
            if (ret != 0) {
                NERROR_FMT_SET(ret, "fail to avcodec_open2 audio codec [{}][{}], err=[{}]",
                                     (int)codec->id, codec->name, av_err2str(ret));
                break;
            }
            
            encodeCfg_.samplefmt = ctx_->sample_fmt;
            encodeCfg_.samplerate = ctx_->sample_rate;
            encodeCfg_.channellayout = ctx_->channel_layout;
            encodeCfg_.framesize = ctx_->frame_size;
            //encodeCfg_.timebase = ctx_->time_base;
            
            srcCfg_.match(encodeCfg_);
            
            ret = converter_.open(srcCfg_, encodeCfg_, timebase_);
            if(ret != 0){
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
        if(ctx_){
            avcodec_free_context(&ctx_);
            ctx_ = nullptr;
        }
        
        if(pkt_){
            av_packet_free(&pkt_);
            pkt_ = nullptr;
        }
    }
    
    int encode(const AVFrame *frame, const FFPacketFunc& out_func){
        int ret = 0;
//        if(frame){
//            odbgd("audio frame: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
//                  , frame->pts, srcRelpts_.offset(frame->pts), srcRelpts_.delta(frame->pts)
//                  , frame->channels, frame->nb_samples, frame->pkt_size
//                  , av_get_sample_fmt_name((AVSampleFormat)frame->format)
//                  , frame->sample_rate);
//        }
        
        ret = converter_.convert(frame, [this, out_func](const AVFrame * outframe)->int{
            return doEncode(outframe, out_func);
        });
        
        if(!frame && numInFrames_ > 0){
            ret = doEncode(nullptr, out_func);
        }
        return ret;
    }
    
private:
    int doEncode(const AVFrame *frame, const FFPacketFunc& out_func){
        if(frame){
            ++numInFrames_;
//            odbgd("No.%lld encode audio: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
//                  , numInFrames_
//                  , frame->pts, encRelpts_.offset(frame->pts), encRelpts_.delta(frame->pts)
//                  , frame->channels, frame->nb_samples, frame->pkt_size
//                  , av_get_sample_fmt_name((AVSampleFormat)frame->format)
//                  , frame->sample_rate);
        }
        int ret = avcodec_send_frame(ctx_, frame);
        if (ret != 0) {
            NERROR_FMT_SET(ret, "fail to avcodec_send_frame audio, err=[{}]",
                                 av_err2str(ret));
            return ret;
        }
        
        while (ret >= 0) {
            ret = avcodec_receive_packet(ctx_, pkt_);
            if (ret == AVERROR(EAGAIN)){
                return 0;
            }else if(ret == AVERROR_EOF){
                return 0;
            }else if (ret < 0) {
                NERROR_FMT_SET(ret, "fail to avcodec_receive_packet audio, err=[{}]",
                                     av_err2str(ret));
                return ret;
            }
            ++numOutPackets_;
//            odbgd("No.%lld audio pkt: pts=%lld(%+lld,%+lld), dts=%lld, size=%d",
//                  numOutPackets_,
//                  pkt_->pts,
//                  dstRelpts_.offset(pkt_->pts),
//                  dstRelpts_.delta(pkt_->pts),
//                  pkt_->dts, pkt_->size);
            ret = out_func(pkt_);
            av_packet_unref(pkt_);
            if(ret){
                return ret;
            }
        }
        return 0;
    }
    
    static int selectSampleRate(const AVCodec *codec, int prefer_samplerate){
        if(!codec->supported_samplerates || *codec->supported_samplerates == 0){
            // no supported samplerate
            if(prefer_samplerate > 0){
                // select prefer one
                return prefer_samplerate;
            }else{
                // select default one
                return 44100;
            }
        }
        
        int best_samplerate = 0;
        const int * p = codec->supported_samplerates;
        while (*p) {
            if (best_samplerate <= 0
                || (abs(prefer_samplerate - *p) < abs(prefer_samplerate - best_samplerate))){
                best_samplerate = *p;
            }
            p++;
        }
        return best_samplerate;
    }
    
    static uint64_t selectChannelLayout(const AVCodec *codec, uint64_t prefer_layout){
        if(!codec->channel_layouts || *codec->channel_layouts == 0){
            // no supported channel layout
            if(prefer_layout > 0){
                // select prefer one
                return prefer_layout; //  == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
            }else{
                // select default one
                return AV_CH_LAYOUT_MONO;
            }
        }
        
        if(prefer_layout == 0){
            // if no prefer, select first
            return *codec->channel_layouts;
        }
        
        int prefer_channels = av_get_channel_layout_nb_channels(prefer_layout);
        uint64_t best_ch_layout = 0;
        int best_nb_channels   = 0;
        const uint64_t *p = codec->channel_layouts;
        while (*p) {
            if(*p == prefer_layout){
                return *p;
            }
            
            int nb_channels = av_get_channel_layout_nb_channels(*p);
            if (best_nb_channels <= 0
                || (abs(prefer_channels - nb_channels) < abs(prefer_channels - best_nb_channels))){
                best_nb_channels = nb_channels;
            }
            p++;
        }
        return best_ch_layout;
    }
    
    static AVSampleFormat selectSampleFormat(const AVCodec *codec, AVSampleFormat prefer_fmt){
        if(!codec->sample_fmts || *codec->sample_fmts <= AV_SAMPLE_FMT_NONE){
            // no supported format
            if(prefer_fmt > AV_SAMPLE_FMT_NONE){
                // select prefer format
                return prefer_fmt;
            }else{
                // select default format
                return AV_SAMPLE_FMT_S16;
            }
        }
        
        const enum AVSampleFormat *p = codec->sample_fmts;
        while (*p != AV_SAMPLE_FMT_NONE) {
            if (*p == prefer_fmt){
                // match, select it
                return prefer_fmt;
            }
            p++;
        }
        // NOT match, select first supported format
        return *codec->sample_fmts;
    }
    
private:
    AVCodecID codecId_ = AV_CODEC_ID_NONE;
    FFSampleConfig  srcCfg_;
    FFSampleConfig  encodeCfg_;
    FFSampleConverter converter_;
    int64_t bitrate_ = -1;
    const AVCodec * codec_ = nullptr;
    AVCodecContext * ctx_ = nullptr;
    AVPacket * pkt_ = nullptr;
    AVRational timebase_;
    int64_t numInFrames_ = 0;
    int64_t numOutPackets_ = 0;
    Int64Relative srcRelpts_;
    Int64Relative encRelpts_;
    Int64Relative dstRelpts_;
};

int main_write_mp4(int argc, char** argv)
{
    av_register_all();
    avcodec_register_all();
    
    const char* video_raw_file = NULL;
    const char* audio_raw_file = NULL;
    const char* output_file = NULL;
    
    output_file = "/tmp/out.mp4";
    video_raw_file = "/Users/simon/Downloads/720p.h264.raw";
    audio_raw_file = "/Users/simon/Downloads/audio.raw.aac";
    
    dbgi("video_raw_file=[%s]", video_raw_file);
    dbgi("audio_raw_file=[%s]", audio_raw_file);
    dbgi("output_file=[%s]", output_file);
    
    //ravrecord_t recorder = nullptr;
    FFContainerWriter writer;
    H264FileReader * video = nullptr;
    AACFileReader * audio = nullptr;
    int ret = 0;
    do{
        if(video_raw_file){
            video = new H264FileReader();
            ret = video->open(video_raw_file, 25);
            if(ret){
                break;
            }
        }
        if(audio_raw_file){
            audio = new AACFileReader();
            ret = audio->open(audio_raw_file, 45);
            if(ret){
                break;
            }
        }
        
        ret = writer.open(output_file,
                           AV_CODEC_ID_H264,
                           1280, 720, 25, // TODO: fps
                           AV_CODEC_ID_AAC,
                           44100,
                           2);
        if (ret) {
            dbge("fail to open writer");
            ret = -1;
            break;
        }
        dbgi("open writer success");
        
        int64_t MAX_TS = 0x00FFFFFFFFFFFFFF;
        int64_t video_ts = MAX_TS;
        int64_t audio_ts = MAX_TS;
        
        if(video){
            video_ts = video->timestamp();
        }
        if(audio){
            audio_ts = audio->timestamp();
        }
        
        while(video_ts != MAX_TS || audio_ts != MAX_TS){
            if(audio && audio_ts <= video_ts && audio_ts != MAX_TS){
                //rr_process_audio(recorder, audio_ts, audio->data(), (int)audio->dataLength());
                writer.writeAudio(audio_ts, audio->data(), (int)audio->dataLength());
                ret = audio->next();
                audio_ts = ret == 0 ? audio->timestamp() : MAX_TS;
            }else if(video && video_ts != MAX_TS){
                
                bool is_key_frame = false;
                int naltype = video->data()[4] & 0x1f;
                if ((naltype == 0x07) || (naltype==0x05) || (naltype == 0x08)) {
                    is_key_frame = true;
                } else {
                    is_key_frame = false;
                }
                writer.writeVideo(video_ts, video->data(), (int)video->dataLength(), is_key_frame);
                ret = video->next();
                video_ts = ret == 0 ? video->timestamp() : MAX_TS;
            }
            
            ret = 0;
        }// loop
        
    }while (0);
    

    writer.close();
    
    if(video){
        delete video;
        video = NULL;
    }
    
    if(audio){
        delete audio;
        audio = NULL;
    }
    
    return 0;
}

int record_camera(){
    const std::string output_path = "/tmp";
    
    const std::string output_encoded_file = output_path +"/out.mp4";
    const std::string deviceName = "FaceTime HD Camera"; // or "2";
    const std::string optFormat = "avfoundation";
    const std::string optPixelFmt;
    FILE * out_yuv_fp = NULL;
    
    
    odbgi("output_encoded_file=[%s]", output_encoded_file.c_str());
    
    FFContainerReader * deviceReader = nullptr;
    FFVideoEncoder videoEncoder;
    FFContainerWriter writer;
    int ret = -1;
    do{
        deviceReader = new FFContainerReader("dev");
        deviceReader->setVideoOptions(640, 480, 30, optPixelFmt);
        ret = deviceReader->open(optFormat, deviceName);
        if(ret){
            odbge("fail to open camera, ret=%d", ret);
            break;
        }
        
        FFVideoTrack * videoTrack = deviceReader->openVideoTrack(AV_PIX_FMT_YUV420P);
        //FFVideoTrack * videoTrack = deviceReader->openVideoTrack(AV_PIX_FMT_NONE);
        if(!videoTrack){
            odbge("fail to open video track");
            ret = -22;
            break;
        }
        odbgi("open camera ok");
        
        FFImageConfig imgcfg;
        imgcfg.pix_fmt = videoTrack->getPixelFormat();
        imgcfg.width = videoTrack->getWidth();
        imgcfg.height = videoTrack->getHeight();
        videoEncoder.setSrcImageConfig(imgcfg);
        videoEncoder.setCodecId(AV_CODEC_ID_H264);
        videoEncoder.setBitrate(1000*1000);
        ret = videoEncoder.open();
        if (ret) {
            odbge("fail to open encoder");
            ret = -1;
            break;
        }
        odbgi("open encoder success");
        
        ret = writer.open(output_encoded_file,
                          AV_CODEC_ID_H264,
                          videoTrack->getWidth(), videoTrack->getHeight(), 25, // TODO: fps
                          AV_CODEC_ID_NONE,
                          44100,
                          2);
        if (ret) {
            odbge("fail to open writer");
            ret = -1;
            break;
        }
        odbgi("open writer success");
        
        const std::string output_yuv_file = fmt::format("{}/out_{}x{}_{}.yuv",
                                                        output_path,
                                                        imgcfg.width,
                                                        imgcfg.height,
                                                        av_get_pix_fmt_name(imgcfg.pix_fmt));
        out_yuv_fp = fopen(output_yuv_file.c_str(), "wb");
        if(!out_yuv_fp){
            odbge("fail to write open [%s]", output_yuv_file.c_str());
            ret = -1;
            break;
        }
        odbgi("opened output_yuv_file=[%s]", output_yuv_file.c_str());
        
        AVRational cap_time_base = {1, 1000000};
        AVRational mux_time_base = {1, 1000};
        FFPTSConverter pts_converter(cap_time_base, mux_time_base);
        
        Int64Relative cap_rel_pts;
        Int64Relative enc_rel_pts;
        
        AVFrame * avframe = NULL;
        for(uint32_t nframes = 0; nframes < 30*3; ++nframes){
            FFTrack * track = deviceReader->readNext();
            if(track->getMediaType() == AVMEDIA_TYPE_VIDEO){
                avframe = track->getLastFrame();
                if(avframe){
                    //avframe->pts = NUtil::get_now_ms();
                    avframe->pts = pts_converter.convert(avframe->pts);
                    odbgi("camera image: pts=%lld(%+lld,%+lld) w=%d, h=%d, size=%d, fmt=%s",
                          avframe->pts, cap_rel_pts.offset(avframe->pts), cap_rel_pts.delta(avframe->pts),
                          avframe->width, avframe->height, avframe->pkt_size,
                          av_get_pix_fmt_name((AVPixelFormat)avframe->format));
                    
                    if(out_yuv_fp ){
                        if(imgcfg.pix_fmt == AV_PIX_FMT_YUV420P){
                            ret = (int)fwrite(avframe->data[0], sizeof(char), imgcfg.width*imgcfg.height, out_yuv_fp);
                            ret = (int)fwrite(avframe->data[1], sizeof(char), imgcfg.width*imgcfg.height/4, out_yuv_fp);
                            ret = (int)fwrite(avframe->data[2], sizeof(char), imgcfg.width*imgcfg.height/4, out_yuv_fp);
                        }else if(imgcfg.pix_fmt == AV_PIX_FMT_UYVY422){
                            ret = (int)fwrite(avframe->data[0], sizeof(char), imgcfg.width*imgcfg.height*2, out_yuv_fp);
                        }

                    }
                    
                    videoEncoder.encode(avframe, [&writer, &enc_rel_pts](AVPacket * pkt)->int{
                        odbgi("encode pkt: pts=%lld(%+lld,%+lld), size=%d, keyfrm=%d",
                              pkt->pts, enc_rel_pts.offset(pkt->pts), enc_rel_pts.delta(pkt->pts),
                              pkt->size,
                              pkt->flags & AV_PKT_FLAG_KEY);
                        return writer.writeVideo(pkt->pts, pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);
                    });
                }
            }
            
        }
        
        odbgi("ecode remain frames");
        videoEncoder.encode(nullptr, [&writer, &enc_rel_pts](AVPacket * pkt)->int{
            odbgi("encode pkt: pts=%lld(%+lld,%+lld), size=%d, keyfrm=%d",
                  pkt->pts, enc_rel_pts.offset(pkt->pts), enc_rel_pts.delta(pkt->pts),
                  pkt->size,
                  pkt->flags & AV_PKT_FLAG_KEY);
            return writer.writeVideo(pkt->pts, pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);
        });
        
        ret = 0;
    }while(0);
    
    deviceReader->close();
    delete deviceReader;
    
    videoEncoder.close();
    writer.close();
    
    if(out_yuv_fp){
        fclose(out_yuv_fp);
        out_yuv_fp = nullptr;
    }
    
    odbgi("record_camera done");
    return ret;
}
int main_record_camera(int argc, char** argv){
    av_register_all();
    avdevice_register_all();
    avcodec_register_all();
    
    return record_camera();
}



int main_record_microphone(int argc, char** argv){
    av_register_all();
    avdevice_register_all();
    avcodec_register_all();
    
    
    const char* output_file = NULL;
    output_file = "/tmp/out.mp4";
    
    
    const std::string deviceName = ":Built-in Microphone"; // or ":0";
    const std::string optFormat = "avfoundation";
    const std::string optPixelFmt;
    
    
    dbgi("output_file=[%s]", output_file);
    
    FFContainerReader deviceReader("device");
    FFAudioEncoder audioEncoder;
    FFContainerWriter writer;
    int ret = -1;
    do{
        ret = deviceReader.open(optFormat, deviceName);
        if(ret){
            odbge("fail to open microphone, ret=%d", ret);
            break;
        }
        
        FFAudioTrack * audioTrack = deviceReader.openAudioTrack(-1, -1, AV_SAMPLE_FMT_NONE);
        //FFAudioTrack * audioTrack = deviceReader.openAudioTrack(-1, -1, AV_SAMPLE_FMT_S16);
        if(!audioTrack){
            odbge("fail to open audio track");
            ret = -22;
            break;
        }
        odbgi("microphone: ch=%d, ar=%d, fmt=[%s], timeb=[%d,%d]",
              audioTrack->getChannels(),
              audioTrack->getSamplerate(),
              av_get_sample_fmt_name(audioTrack->getSampleFormat()),
              audioTrack->getTimebase().num, audioTrack->getTimebase().den );
        
        AVRational cap_time_base = {1, 1000000};
        AVRational mux_time_base = {1, 1000};
        FFPTSConverter pts_converter(cap_time_base, mux_time_base);
        
        FFSampleConfig src_cfg;
        //src_cfg.timebase = cap_time_base;
        src_cfg.samplerate = audioTrack->getSamplerate();
        src_cfg.samplefmt = audioTrack->getSampleFormat();
        src_cfg.channellayout = av_get_default_channel_layout(audioTrack->getChannels()); // TODO:
        
        audioEncoder.setCodecName("libfdk_aac"); //audioEncoder.setCodecId(AV_CODEC_ID_AAC);
        audioEncoder.setSrcConfig(src_cfg);
        audioEncoder.setTimebase(mux_time_base);
        ret = audioEncoder.open();
        if (ret) {
            odbge("fail to open encoder");
            ret = -1;
            break;
        }
        odbgi("open encoder success");
        
        ret = writer.open(output_file,
                          AV_CODEC_ID_NONE,
                          0, 0, 0,
                          AV_CODEC_ID_AAC,
                          audioEncoder.encodeConfig().samplerate,
                          av_get_channel_layout_nb_channels(audioEncoder.encodeConfig().channellayout));
        if (ret) {
            odbge("fail to open writer");
            ret = -1;
            break;
        }
        odbgi("open writer success");
        
        int64_t start_ms = NUtil::get_now_ms();
        AVFrame * avframe = NULL;
        for(uint32_t nframes = 0; (NUtil::get_now_ms()-start_ms) < 5000; ++nframes){
            FFTrack * track = deviceReader.readNext();
            if(track->getMediaType() == AVMEDIA_TYPE_AUDIO){
                avframe = track->getLastFrame();
                if(avframe){
                    avframe->pts = pts_converter.convert(avframe->pts);
                    ret = audioEncoder.encode(avframe, [&writer](AVPacket * pkt)->int{
                        return writer.writeAudio(pkt->pts, pkt->data, pkt->size);
                    });
                    if(ret){
                        odbge("fail to encode audio, err=[%d]-[%s]",
                              ret, NThreadError::lastMsg().c_str());
                        break;
                    }
                }
            }
        }
        
        if(ret == 0){
            odbgi("encode remain frames");
            ret = audioEncoder.encode(nullptr, [&writer](AVPacket * pkt)->int{
                return writer.writeAudio(pkt->pts, pkt->data, pkt->size);
            });
            if(ret){
                odbge("fail to encode audio, err=[%d]-[%s]",
                      ret, NThreadError::lastMsg().c_str());
                break;
            }
        }
        
        ret = 0;
    }while(0);
    
    deviceReader.close();
    audioEncoder.close();
    writer.close();
    
    return 0;
}

int main_record_mp4(int argc, char** argv){
    av_register_all();
    avdevice_register_all();
    avcodec_register_all();

    const std::string video_codec_name = "libx264";
    const std::string audio_codec_name = "libfdk_aac";
    
    const std::string deviceName = "FaceTime HD Camera:Built-in Microphone"; // or "0:0";
    //const std::string deviceName = "FaceTime HD Camera"; // camera only
    //const std::string deviceName = ":Built-in Microphone"; // microphone only
    const std::string optFormat = "avfoundation";
    const std::string optPixelFmt;
    
    const std::string output_path = "/tmp";
    const std::string output_encoded_file = output_path +"/out.mp4";
    FILE * out_yuv_fp = NULL;
    

    odbgi("output_encoded_file=[%s]", output_encoded_file.c_str());

    FFContainerReader deviceReader("device");
    FFAudioEncoder audioEncoder;
    FFVideoEncoder videoEncoder;
    FFContainerWriter writer;
    int ret = -1;
    do{
        AVRational cap_time_base = {1, 1000000};
        AVRational mux_time_base = {1, 1000};
        FFPTSConverter pts_converter(cap_time_base, mux_time_base);
        
        //deviceReader.setVideoOptions(640, 480, 30, optPixelFmt);
        deviceReader.setVideoOptions(1280, 720, 30, optPixelFmt);
        ret = deviceReader.open(optFormat, deviceName);
        if(ret){
            odbge("fail to open microphone, ret=%d", ret);
            break;
        }

        FFAudioTrack * audioTrack = deviceReader.openAudioTrack(-1, -1, AV_SAMPLE_FMT_NONE);
        //FFAudioTrack * audioTrack = deviceReader.openAudioTrack(-1, -1, AV_SAMPLE_FMT_S16);
        if(!audioTrack){
            odbgi("no audio track");
        }else{
            odbgi("opened audio: ch=%d, ar=%d, fmt=[%s], timeb=[%d,%d]",
                  audioTrack->getChannels(),
                  audioTrack->getSamplerate(),
                  av_get_sample_fmt_name(audioTrack->getSampleFormat()),
                  audioTrack->getTimebase().num, audioTrack->getTimebase().den );
            
            FFSampleConfig sample_cfg;
            sample_cfg.samplerate = audioTrack->getSamplerate();
            sample_cfg.samplefmt = audioTrack->getSampleFormat();
            sample_cfg.channellayout = av_get_default_channel_layout(audioTrack->getChannels()); // TODO:
            
            //audioEncoder.setCodecId(AV_CODEC_ID_AAC);
            audioEncoder.setCodecName(audio_codec_name);
            audioEncoder.setSrcConfig(sample_cfg);
            audioEncoder.setTimebase(mux_time_base);
            ret = audioEncoder.open();
            if (ret) {
                odbge("fail to open encoder");
                ret = -1;
                break;
            }
            odbgi("open audio encoder success");
        }
        
        FFImageConfig imgcfg;
        FFVideoTrack * videoTrack = deviceReader.openVideoTrack(AV_PIX_FMT_NONE);
        //FFVideoTrack * videoTrack = deviceReader.openVideoTrack(AV_PIX_FMT_YUV420P);
        if(!videoTrack){
            odbgi("no video track");
        }else{
            odbgi("opened video: size=%dx%d, fmt=[%s], timeb=[%d,%d]",
                  videoTrack->getWidth(), videoTrack->getHeight(),
                  av_get_pix_fmt_name(videoTrack->getPixelFormat()),
                  videoTrack->getTimebase().num, videoTrack->getTimebase().den );
            
            imgcfg.pix_fmt = videoTrack->getPixelFormat();
            imgcfg.width = videoTrack->getWidth();
            imgcfg.height = videoTrack->getHeight();
            videoEncoder.setSrcImageConfig(imgcfg);
            //videoEncoder.setCodecId(AV_CODEC_ID_H264);
            videoEncoder.setCodecName(video_codec_name);
            videoEncoder.setBitrate(2000*1000);
            videoEncoder.setCodecOpt("preset", "fast");
            ret = videoEncoder.open();
            if (ret) {
                odbge("fail to open encoder");
                ret = -1;
                break;
            }
            odbgi("open video encoder success");
            
            const std::string output_yuv_file = fmt::format("{}/out_{}x{}_{}.yuv",
                                                            output_path,
                                                            imgcfg.width,
                                                            imgcfg.height,
                                                            av_get_pix_fmt_name(imgcfg.pix_fmt));
            out_yuv_fp = fopen(output_yuv_file.c_str(), "wb");
            if(!out_yuv_fp){
                odbge("fail to write open [%s]", output_yuv_file.c_str());
                ret = -1;
                break;
            }
            odbgi("opened output_yuv_file=[%s]", output_yuv_file.c_str());
        }
        
        if(!audioTrack && !videoTrack){
            odbge("empty track");
            ret = -22;
            break;
        }
        
        ret = writer.open(output_encoded_file,
                          videoEncoder.codecId(),
                          videoEncoder.encodeImageCfg().width,
                          videoEncoder.encodeImageCfg().height,
                          25,
                          audioEncoder.codecId(),
                          audioEncoder.encodeConfig().samplerate,
                          av_get_channel_layout_nb_channels(audioEncoder.encodeConfig().channellayout));
        if (ret) {
            odbge("fail to open writer");
            ret = -1;
            break;
        }
        odbgi("open writer success");

        Int64Relative audio_cap_pts;
        Int64Relative audio_enc_pts;
        
        Int64Relative video_cap_pts;
        Int64Relative video_enc_pts;

        int64_t start_ms = NUtil::get_now_ms();
        AVFrame * avframe = NULL;
        for(uint32_t nframes = 0; (NUtil::get_now_ms()-start_ms) < 5000; ++nframes){
            FFTrack * track = deviceReader.readNext();
            if(!track){
                odbgi("reach device end");
                break;
            }
            avframe = track->getLastFrame();
            if(!avframe){
                continue;
            }

            if(track->getMediaType() == AVMEDIA_TYPE_AUDIO){
                avframe->pts = pts_converter.convert(avframe->pts);
                odbgi("audio frame: pts=%lld(%+lld,%+lld), samplerate=%d, ch=%d, nb_samples=%d, fmt=[%s], pkt_size=%d",
                      avframe->pts, audio_cap_pts.offset(avframe->pts), audio_cap_pts.delta(avframe->pts),
                      avframe->sample_rate, avframe->channels, avframe->nb_samples,
                      av_get_sample_fmt_name((AVSampleFormat)avframe->format),
                      avframe->pkt_size);
                ret = audioEncoder.encode(avframe, [&writer, &audio_enc_pts](AVPacket * pkt)->int{
                    odbgi("write audio: pts=%lld(%+lld,%+lld), dts=%lld, size=%d",
                          pkt->pts, audio_enc_pts.offset(pkt->pts), audio_enc_pts.delta(pkt->pts), pkt->dts, pkt->size);
                    return writer.writeAudio(pkt->pts, pkt->data, pkt->size);
                });
                
                if(ret){
                    odbge("fail to encode audio, err=[%d]-[%s]",
                          ret, NThreadError::lastMsg().c_str());
                    break;
                }
                
            }else if(track->getMediaType() == AVMEDIA_TYPE_VIDEO){
                //avframe->pts = NUtil::get_now_ms();
                avframe->pts = pts_converter.convert(avframe->pts);
                odbgi("video frame: pts=%lld(%+lld,%+lld), w=%d, h=%d, fmt=[%s], size=%d",
                      avframe->pts, video_cap_pts.offset(avframe->pts), video_cap_pts.delta(avframe->pts),
                      avframe->width, avframe->height,
                      av_get_pix_fmt_name((AVPixelFormat)avframe->format),
                      avframe->pkt_size);
                
                if(out_yuv_fp ){
                    if(imgcfg.pix_fmt == AV_PIX_FMT_YUV420P){
                        ret = (int)fwrite(avframe->data[0], sizeof(char), imgcfg.width*imgcfg.height, out_yuv_fp);
                        ret = (int)fwrite(avframe->data[1], sizeof(char), imgcfg.width*imgcfg.height/4, out_yuv_fp);
                        ret = (int)fwrite(avframe->data[2], sizeof(char), imgcfg.width*imgcfg.height/4, out_yuv_fp);
                    }else if(imgcfg.pix_fmt == AV_PIX_FMT_UYVY422){
                        ret = (int)fwrite(avframe->data[0], sizeof(char), imgcfg.width*imgcfg.height*2, out_yuv_fp);
                    }
                    
                }
                
                ret = videoEncoder.encode(avframe, [&writer, &video_enc_pts](AVPacket * pkt)->int{
                    odbgi("write video pkt: pts=%lld(%+lld,%+lld), dts=%lld, size=%d, keyfrm=%d",
                          pkt->pts, video_enc_pts.offset(pkt->pts), video_enc_pts.delta(pkt->pts),
                          pkt->dts, pkt->size,
                          pkt->flags & AV_PKT_FLAG_KEY);
                    return writer.writeVideo(pkt->pts, pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);
                });
                
                if(ret){
                    odbge("fail to encode video, err=[%d]-[%s]",
                          ret, NThreadError::lastMsg().c_str());
                    break;
                }
            }
        }

        odbgi("ecode remain frames");
        
        if(audioTrack){
            ret = audioEncoder.encode(nullptr, [&writer, &audio_enc_pts](AVPacket * pkt)->int{
                odbgi("write audio pkt: pts=%lld(%+lld,%+lld), dts=%lld, size=%d",
                      pkt->pts, audio_enc_pts.offset(pkt->pts), audio_enc_pts.delta(pkt->pts), pkt->dts, pkt->size);
                return writer.writeAudio(pkt->pts, pkt->data, pkt->size);
            });
        }

        if(videoTrack){
            ret = videoEncoder.encode(nullptr, [&writer, &video_enc_pts](AVPacket * pkt)->int{
                odbgi("write video pkt: pts=%lld(%+lld,%+lld), dts=%lld, size=%d, keyfrm=%d",
                      pkt->pts, video_enc_pts.offset(pkt->pts), video_enc_pts.delta(pkt->pts),
                      pkt->dts, pkt->size,
                      pkt->flags & AV_PKT_FLAG_KEY);
                return writer.writeVideo(pkt->pts, pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);
            });
        }


        ret = 0;
    }while(0);

    deviceReader.close();
    audioEncoder.close();
    videoEncoder.close();
    writer.close();
    
    if(out_yuv_fp){
        fclose(out_yuv_fp);
        out_yuv_fp = nullptr;
    }
    
    odbgi("record done");
    
    return ret;
}


int lab030_rtmp_push_main(int argc, char** argv){
    //return main_push_av(argc, argv);
    //return main_write_mp4(argc, argv);
    //return main_record_camera(argc, argv);
    //return main_record_microphone(argc, argv);
    return main_record_mp4(argc, argv);
}
