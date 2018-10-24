


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

int lab030_rtmp_push_main(int argc, char** argv){
    return main_push_av(argc, argv);
}
