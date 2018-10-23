
#include "SDLFramework.hpp"

#include <string>
#include <map>
#include <vector>
#include <list>
#include <sstream>
#include <random>
#include <chrono>
#include <thread>
#include <stdio.h>

#include <unistd.h>
#include <opus/opus.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

extern "C"{
    #include "vpx/vpx_encoder.h"
    #include "vpx/vp8cx.h"
    #include "vpx/vpx_decoder.h"
    #include "vpx/vp8dx.h"
}


#include "Lab041-BStreamTLVPlayer.hpp"
#include "xtlv_file.h"
#include "BStreamProto.h"


#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)

using namespace rapidjson;

static
int64_t get_now_ms(){
    unsigned long milliseconds_since_epoch =
    std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count();
    return milliseconds_since_epoch;
};

class BStreamTLVPlayer{
    FILE * fp_ = NULL;
    uint8_t * buf_ = NULL;
    size_t bufSize_ = 0;
    int64_t dataTimestamp_ = 0;
    int dataType_ = 0;
    uint8_t * dataPtr_ = NULL;
    size_t dataLength_ = 0;

public:
    int open(const std::string& filename){
        int ret = 0;
        do{
            this->close();
            fp_ = fopen(filename.c_str(), "rb");
            if (!fp_) {
                odbge("fail to open file [%s]", filename.c_str());
                ret = -1;
                break;
            }
            bufSize_ = 16*1024;
            buf_ = (uint8_t*)malloc(bufSize_);
            dataPtr_ = buf_ + sizeof(int64_t);
            ret = readNext();
        }while (0);
        if(ret){
            this->close();
        }
        return ret;
    }
    void close(){
        if(fp_){
            fclose(fp_);
            fp_ = NULL;
        }
        if(buf_){
            free(buf_);
            buf_ = NULL;
        }
        bufSize_ = 0;
    }
    
    int readNext(){
        int ret = 0;
        do{
            int bytesRead = 0;
            ret = tlv_file_read_raw(fp_, buf_, (int)bufSize_, &bytesRead, &dataType_);
            if(ret){
                break;
            }
            if(bytesRead < sizeof(int64_t)){
                ret = -1;
                break;
            }
            dataTimestamp_ = be_get_i64(buf_);
            dataLength_ = bytesRead - sizeof(int64_t);
            dataPtr_[dataLength_] = 0;
        }while (0);
        return ret;
    }
    
    void demuxRTP(uint8_t *data, size_t size, int& rtp_pt, int& rtp_seq, uint8_t *& rtp_payload, size_t&rtp_payload_len){
        
        // see RFC3550 for rtp
        bool ext_exist = (data[0]>>4&1) == 1;
        int num_csrcs = data[0]&0xf;
        int header_size = 12 + num_csrcs*4;
        rtp_pt = data[1]&0x7f;
        rtp_seq = data[2]<<8 | data[3];
        
        // see RFC5285 for rtp header extensions
        int ext_size = 0;
        if (ext_exist) {
            ext_size = (data[header_size+2] << 8) | (data[header_size+3]);
            ext_size = ext_size*4 + 4;
        }
        
        rtp_payload = data+header_size+ext_size;
        rtp_payload_len = size-header_size-ext_size;
        odbgi("  rtp: pt=%d, seq=%d, payload_len=%ld", rtp_pt, rtp_seq, rtp_payload_len);
        
    }
    
    void play(const std::string& filename){
        int ret = 0;
        do {
            ret = this->open(filename);
            if(ret) break;
            
            int samplerate = 16000;
            int channels = 1;
            int samplesPerCh = 1024;
            SDLAudioPlayer player;
            ret = player.open(samplerate, channels, samplesPerCh);
            if(ret){
                odbge("can't open SDL audio, ret=%d", ret);
                break;
            }
            player.play(true);
            
            int activePort_ = 0;
            OpusDecoder * audioDecoder_ = NULL;
            size_t pcm_buf_size = 96000;
            int16_t * pcm_buf = (int16_t *)malloc(pcm_buf_size*sizeof(int16_t));
            
            int64_t start_ms = get_now_ms();
            int64_t ts_start = dataTimestamp_;
            while(ret == 0){
                int64_t elapsed_ms = get_now_ms() - start_ms;
                int64_t relative_time_ms = dataTimestamp_ - ts_start;
                int64_t ms = relative_time_ms - elapsed_ms;
                if(ms > 0){
                    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                }
                odbgi("type=[%d], ts=[%lld], len=%ld", dataType_, dataTimestamp_, dataLength_);
                if(dataType_ == TLV_TYPE_TIME_STR){
                    odbgi("  vstr=[%s]", (char*)dataPtr_);
                }else if(dataType_ == TLV_TYPE_TIME_PORT_ADD){
                    int port = be_get_u32(dataPtr_);
                    odbgi("  port-add=[%d]", port);
                    if(activePort_ == 0){
                        activePort_ = port;
                        audioDecoder_ = opus_decoder_create(samplerate, channels, &ret);
                    }
                }else if(dataType_ == TLV_TYPE_TIME_PORT_REMOVE){
                    int port = be_get_u32(dataPtr_);
                    odbgi("  port-remove=[%d]", port);
                    if(port == activePort_){
                        opus_decoder_destroy(audioDecoder_);
                        audioDecoder_ = NULL;
                        activePort_ = 0;
                    }
                }else if(dataType_ == TLV_TYPE_TIME_PORT_RECVUDP){
                    int port = be_get_u32(dataPtr_);
                    int rtp_pt = 0;
                    int rtp_seq = 0;
                    uint8_t * rtp_payload = NULL;
                    size_t rtp_payload_len  =0;
                    demuxRTP(dataPtr_+4, dataLength_-4, rtp_pt, rtp_seq, rtp_payload, rtp_payload_len);
                    odbgi("  port-recv=[%d]-[%d]", port, (int)dataLength_-4);
                    const int OPUS_RTP_PT = 103;
                    if(port == activePort_ && rtp_payload_len > 0){
                        if (rtp_pt == OPUS_RTP_PT) {
                            int frame_size = opus_decode(audioDecoder_
                                                         , rtp_payload, (int)rtp_payload_len
                                                         , pcm_buf, (int)pcm_buf_size
                                                         , 0);
                            odbgi("audio decode pcm samples %d", frame_size*channels);
                            player.queueSamples(pcm_buf, frame_size*channels);
                        }

                    }
                }
                
                ret = this->readNext();
            }
            odbgi("waiting for playing complete");
            player.waitForComplete();
            odbgi("play complete");
            
            if(pcm_buf){
                free(pcm_buf);
                pcm_buf = NULL;
            }
            if(audioDecoder_){
                opus_decoder_destroy(audioDecoder_);
                audioDecoder_ = NULL;
            }
            
        } while (0);
    }
};

int lab_bstream_tlv_player_main(int argc, char* argv[]){
    odbgi("hello");
    int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) ;
    if(ret){
        odbge( "fail too init SDL, err=[%s]", SDL_GetError());
        return -1;
    }
    ret = TTF_Init();
    
    
//    std::string filepath = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/LearningRTC/data/test_s16le_44100Hz_2ch.pcm";
//    playPCMFile(filepath.c_str(), 44100, 2);
    
    BStreamTLVPlayer tlvPlayer;
    tlvPlayer.play("/Users/simon/Downloads/branch_stream.tlv");
    
    odbgi("bye!");
    
    TTF_Quit();
    SDL_Quit();
    return ret;
}


