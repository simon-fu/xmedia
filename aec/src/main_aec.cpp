#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h> // SIGINT
#include <list>
#include <string>

#include "webrtc/modules/audio_processing/aecm/echo_control_mobile.h"
#include "webrtc/modules/audio_processing/aec/echo_cancellation.h"

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include "xcutil.h"
#include "xwavfile.h"
#include "sabine_aec.h"

//#define dbgd(...) do{  printf("<aec>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
//#define dbgi(...) do{  printf("<aec>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
//#define dbge(...) do{  printf("<aec>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)



// 08-12 12:51:55.198 10459 10683 I alog    : WebRtcAecm_Init: aecmInst=0x610c0a80, sampFreq=8000
// 08-12 12:51:55.198 10459 10683 I alog    : WebRtcAecm_set_config: aecmInst=0x610c0a80, config.cngMode=1, config.echoMode=3
// 08-12 12:51:55.198 10459 10683 I alog    : WebRtcAecm_set_config: aecmInst=0x610c0a80, config.cngMode=0, config.echoMode=3

// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_BufferFarend: aecmInst=0x626f1b18, farend=0x627011b8, nrOfSamples=160
// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_GetBufferFarendError: aecmInst=0x626f1b18, farend=0x627011b8, nrOfSamples=160
// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_BufferFarend: aecmInst=0x626f1b18, farend=0x62701300, nrOfSamples=160
// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_GetBufferFarendError: aecmInst=0x626f1b18, farend=0x62701300, nrOfSamples=160
// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_BufferFarend: aecmInst=0x626f1b18, farend=0x62701448, nrOfSamples=160
// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_GetBufferFarendError: aecmInst=0x626f1b18, farend=0x62701448, nrOfSamples=160

// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_Process: aecmInst=0x626f1b18, nearendNoisy=0x65193568, nearendClean=0x626f1368, out=0x626f1368, nrOfSamples=160, msInSndCardBuf=150
// 08-12 12:42:13.848  9797 10170 I alog    : WebRtcAecm_Process: aecmInst=0x626f1b18, nearendNoisy=0x65193568, nearendClean=0x626f1368, out=0x626f1368, nrOfSamples=160, msInSndCardBuf=150
// 08-12 12:42:13.858  9797 10170 I alog    : WebRtcAecm_Process: aecmInst=0x626f1b18, nearendNoisy=0x65193568, nearendClean=0x626f1368, out=0x626f1368, nrOfSamples=160, msInSndCardBuf=150
// 08-12 12:42:13.868  9797 10170 I alog    : WebRtcAecm_Process: aecmInst=0x626f1b18, nearendNoisy=0x65193568, nearendClean=0x626f1368, out=0x626f1368, nrOfSamples=160, msInSndCardBuf=150


//static
//int run_aecm(wavfile_reader_t readerNear
//        , wavfile_reader_t readerFar
//        , wavfile_writer_t writerOut
//        , int32_t sampFreq
//        , int nrOfSamples
//        , int msInSndCardBuf
//        , int16_t near_buf[]
//        , int16_t far_buf[]
//        , int16_t out_buf[]
//        , int framebytes
//        , int& frameCount){
//
//    int ret = 0;
//    void* aecmInst = NULL;
//    AecmConfig aecmConfig = {.cngMode=0, .echoMode=3};
//
//    do{
//
//        aecmInst = WebRtcAecm_Create();
//        if(!aecmInst){
//            dbge("WebRtcAecm_Create fail");
//            break;
//        }
//        dbgi("WebRtcAecm_Create success, aecmInst=%p", aecmInst);
//
//        ret = WebRtcAecm_Init(aecmInst, sampFreq);
//        if(ret != 0){
//            dbge("WebRtcAecm_Init fail ret=%d", ret);
//            break;
//        }
//        dbgi("WebRtcAecm_Init success");
//
//        ret = WebRtcAecm_set_config(aecmInst, aecmConfig);
//        if(ret != 0){
//            dbge("WebRtcAecm_set_config fail ret=%d, config=[%d, %d]", ret, aecmConfig.cngMode, aecmConfig.echoMode);
//            break;
//        }
//        dbgi("WebRtcAecm_set_config success, config=[.cngMode=%d, .echoMode=%d]", aecmConfig.cngMode, aecmConfig.echoMode);
//
//
//
//
//        frameCount = 0;
//        while(1){
//            ret = wavfile_reader_read(readerFar, far_buf, framebytes);
//            if(ret != framebytes){
//                dbgi("reach far-end file end");
//                ret = 0;
//                break;
//            }
//
//            ret = WebRtcAecm_BufferFarend(aecmInst, far_buf, nrOfSamples);
//            if(ret != 0){
//                dbge("WebRtcAecm_BufferFarend fail ret=%d", ret);
//                break;
//            }
//
//            ret = wavfile_reader_read(readerNear, near_buf, framebytes);
//            if(ret != framebytes){
//                dbgi("reach near-end file end");
//                ret = 0;
//                break;
//            }
//
//            ret = WebRtcAecm_Process(aecmInst,
//                                       near_buf, // nearendNoisy,
//                                       near_buf, // nearendClean,
//                                       out_buf,
//                                       nrOfSamples,
//                                       msInSndCardBuf);
//            if(ret != 0){
//                dbge("WebRtcAecm_Process fail ret=%d", ret);
//                break;
//            }
//
//            //memcpy(out_buf, near_buf, framebytes);
//            ret = wavfile_writer_write(writerOut, out_buf, framebytes);
//            if(ret < 0){
//                dbge("wavfile_writer_write fail ret=%d", ret);
//                break;
//            }
//
//            ret = 0;
//            frameCount++;
//        }
//
//    }while(0);
//
//    if(aecmInst){
//        dbgi("WebRtcAecm_Free aecmInst=%p", aecmInst);
//        WebRtcAecm_Free(aecmInst);
//        aecmInst = NULL;
//    }
//
//    return ret;
//}
//
//static
//inline void copy_int16_to_float(int16_t src[], float dst[], int num){
//    for(int i = 0; i < num; i++){
//        dst[i] = src[i];
//    }
//}
//
//static
//inline void copy_float_to_int16(float src[], int16_t dst[], int num){
//    for(int i = 0; i < num; i++){
//        dst[i] = src[i];
//    }
//}
//
//static
//int run_aec(wavfile_reader_t readerNear
//        , wavfile_reader_t readerFar
//        , wavfile_writer_t writerOut
//        , int32_t sampFreq
//        , int nrOfSamples
//        , int msInSndCardBuf
//        , int16_t near_buf[]
//        , int16_t far_buf[]
//        , int16_t out_buf[]
//        , int framebytes
//        , int& frameCount){
//
//    int ret = 0;
//    void* aInst = NULL;
//    // AecConfig aConfig = {.nlpMode=kAecNlpModerate, .skewMode=kAecFalse, .metricsMode=kAecFalse, .delay_logging=kAecFalse};
//    // AecConfig aConfig = {.nlpMode=kAecNlpAggressive, .skewMode=kAecFalse, .metricsMode=kAecFalse, .delay_logging=kAecFalse};
//    AecConfig config = {.nlpMode=kAecNlpConservative, .skewMode=kAecFalse, .metricsMode=kAecFalse, .delay_logging=kAecFalse};
//    AecConfig * aConfig = NULL; // &config;
//
//    do{
//
//        aInst = WebRtcAec_Create();
//        if(!aInst){
//            dbge("WebRtcAec_Create fail");
//            break;
//        }
//        dbgi("WebRtcAec_Create success, aInst=%p", aInst);
//
//        ret = WebRtcAec_Init(aInst, sampFreq, sampFreq);
//        if(ret != 0){
//            dbge("WebRtcAec_Init fail ret=%d", ret);
//            break;
//        }
//        dbgi("WebRtcAec_Init success");
//
//        if(aConfig){
//            ret = WebRtcAec_set_config(aInst, *aConfig);
//            if(ret != 0){
//                dbge("WebRtcAec_set_config fail ret=%d, config=[%d, %d, %d, %d]", ret, aConfig->nlpMode, aConfig->skewMode, aConfig->metricsMode, aConfig->delay_logging);
//                break;
//            }
//            dbgi("WebRtcAec_set_config success, config=[%d, %d, %d, %d]", aConfig->nlpMode, aConfig->skewMode, aConfig->metricsMode, aConfig->delay_logging);
//        }else{
//            dbgi("skip set config");
//        }
//
//
//
//        float far_data[nrOfSamples] ;
//        float near_data[nrOfSamples] ;
//        float out_data[nrOfSamples] ;
//
//        frameCount = 0;
//        while(1){
//            ret = wavfile_reader_read(readerFar, far_buf, framebytes);
//            if(ret != framebytes){
//                dbgi("reach far-end file end");
//                ret = 0;
//                break;
//            }
//            copy_int16_to_float(far_buf, far_data, nrOfSamples);
//
//            ret = WebRtcAec_BufferFarend(aInst, far_data, nrOfSamples);
//            if(ret != 0){
//                dbge("WebRtcAecm_BufferFarend fail ret=%d", ret);
//                break;
//            }
//
//            ret = wavfile_reader_read(readerNear, near_buf, framebytes);
//            if(ret != framebytes){
//                dbgi("reach near-end file end");
//                ret = 0;
//                break;
//            }
//            copy_int16_to_float(near_buf, near_data, nrOfSamples);
//            float * nearend[1]  = {near_data};
//            float * out[1]  = {out_data};
//            ret = WebRtcAec_Process(aInst,
//                                       nearend, // nearend,
//                                       1, // ,
//                                       out,
//                                       nrOfSamples,
//                                       msInSndCardBuf,
//                                       0);
//            if(ret != 0){
//                dbge("WebRtcAec_Process fail ret=%d", ret);
//                break;
//            }
//            copy_float_to_int16(out_data, out_buf, nrOfSamples);
//
//            //memcpy(out_buf, near_buf, framebytes);
//            ret = wavfile_writer_write(writerOut, out_buf, framebytes);
//            if(ret < 0){
//                dbge("wavfile_writer_write fail ret=%d", ret);
//                break;
//            }
//
//            ret = 0;
//            frameCount++;
//        }
//
//    }while(0);
//
//    if(aInst){
//        dbgi("WebRtcAec_Free aInst=%p", aInst);
//        WebRtcAec_Free(aInst);
//        aInst = NULL;
//    }
//
//    return ret;
//}
//
//
//static
//int run_speex(wavfile_reader_t readerNear
//        , wavfile_reader_t readerFar
//        , wavfile_writer_t writerOut
//        , int32_t sampFreq
//        , int nrOfSamples
//        , int msInSndCardBuf
//        , int16_t near_buf[]
//        , int16_t far_buf[]
//        , int16_t out_buf[]
//        , int framebytes
//        , int& frameCount){
//
//    int ret = 0;
//
//    SpeexPreprocessState *preprocess_state = NULL;
//    SpeexEchoState *echo_state = NULL;
//    int  preprocess_on = 1;
//
//    nrOfSamples = 2 * sampFreq/100; // 20 ms
//    framebytes = nrOfSamples * 2;
//    int frame_size = nrOfSamples;
//    int filter_length =  sampFreq/10/2; // 100 ms
//
//    dbgi("use speex echo (preprocess_on=%d)", preprocess_on);
//    dbgi("frame_size=%d", frame_size);
//    dbgi("filter_length=%d", filter_length);
//
//    do{
//
//        echo_state = speex_echo_state_init(frame_size, filter_length);
//        if(!echo_state){
//            dbge("speex_echo_state_init fail");
//            break;
//        }
//        dbgi("speex_echo_state_init success, obj=%p", echo_state);
//
//        int sampleRate = sampFreq;
//        speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
//
//        if(preprocess_on){
//            preprocess_state = speex_preprocess_state_init(frame_size, sampFreq);
//            if(!preprocess_state){
//                dbge("speex_preprocess_state_init fail");
//                break;
//            }
//            dbgi("speex_preprocess_state_init success, obj=%p", preprocess_state);
//
//
//            ret = speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state);
//            if(ret != 0){
//                dbge("speex_preprocess_ctl (SPEEX_PREPROCESS_SET_ECHO_STATE) fail, ret=%d", ret);
//                break;
//            }
//            dbge("speex_preprocess_ctl (SPEEX_PREPROCESS_SET_ECHO_STATE) success");
//        }
//
//
//
//        frameCount = 0;
//        while(1){
//            ret = wavfile_reader_read(readerFar, far_buf, framebytes);
//            if(ret != framebytes){
//                dbgi("reach far-end file end");
//                ret = 0;
//                break;
//            }
//
//            ret = wavfile_reader_read(readerNear, near_buf, framebytes);
//            if(ret != framebytes){
//                dbgi("reach near-end file end");
//                ret = 0;
//                break;
//            }
//
//            // if(preprocess_state){
//            //     speex_preprocess_run(preprocess_state, near_buf);
//            // }
//            speex_preprocess_run(preprocess_state, near_buf);
//            speex_echo_playback(echo_state, far_buf);
//            speex_echo_capture(echo_state, near_buf, out_buf);
//
//
//            // speex_echo_cancellation(echo_state, far_buf, near_buf, out_buf);
//            // speex_preprocess_run(preprocess_state, out_buf);
//
//
//
//            //memcpy(out_buf, near_buf, framebytes);
//            ret = wavfile_writer_write(writerOut, out_buf, framebytes);
//            if(ret < 0){
//                dbge("wavfile_writer_write fail ret=%d", ret);
//                break;
//            }
//
//            ret = 0;
//            frameCount++;
//        }
//
//    }while(0);
//
//    if(preprocess_state){
//        dbgi("speex_preprocess_state_destroy aInst=%p", preprocess_state);
//        speex_preprocess_state_destroy(preprocess_state);
//        preprocess_state = NULL;
//    }
//
//    if(echo_state){
//        dbgi("speex_echo_state_destroy aInst=%p", echo_state);
//        speex_echo_state_destroy(echo_state);
//        echo_state = NULL;
//    }
//
//    return ret;
//}
//
//
//
//int aec_wave_main(int argc, char** argv){
//
//    // const char * fileNear = "tmp/input.wav"; // "tmp/input.wav"; "tmp/mix.wav";
//    // const char * fileFar = "tmp/reverse.wav";
//    // const char * fileOut = "tmp/out.wav";
//
//    const char * fileNear = "tmp/input.wav"; // "tmp/input.wav"; "tmp/mix.wav";
//    const char * fileFar = "tmp/reverse.wav";
//    const char * fileOut = "tmp/out.wav";
//
//    int ret = 0;
//    wavfile_reader_t readerNear = NULL;
//    wavfile_reader_t readerFar = NULL;
//    wavfile_writer_t writerOut = NULL;
//
//    int32_t sampFreq = 16000;
//    int nrOfSamples = 160;
//
//    // int msInSndCardBuf = 150; // milliseconds
//    int msInSndCardBuf = 150; // milliseconds
//    // int msInSndCardBuf = 300; // milliseconds
//
//    int dropMilliSeconds = 300;
//
//    int16_t near_buf[nrOfSamples*10];
//    int16_t far_buf[nrOfSamples*10];
//    int16_t out_buf[nrOfSamples*10];
//
//    if(argc > 3){
//        fileNear = argv[1];
//        fileFar = argv[2];
//        fileOut = argv[3];
//    }
//
//    if(argc > 4){
//        dropMilliSeconds = atoi(argv[4]);
//    }
//
//    if(argc > 5){
//        msInSndCardBuf = atoi(argv[5]);
//    }
//
//
//    dbgi("usage: %s [fileNear] [fileFar] [fileOut] [dropMilliSeconds] [msInSndCardBuf]", argv[0]);
//    dbgi("fileNear = %s", fileNear);
//    dbgi("fileFar = %s", fileFar);
//    dbgi("fileOut = %s", fileOut);
//
//    dbgi("sampFreq = %d", sampFreq);
//    dbgi("nrOfSamples = %d", nrOfSamples);
//    dbgi("msInSndCardBuf = %d", msInSndCardBuf);
//    dbgi("dropMilliSeconds = %d", dropMilliSeconds);
//
//    do{
//        readerNear = wavfile_reader_open(fileNear);
//        if(!readerNear){
//            dbge("fail to open %s", fileNear);
//            ret = -1;
//            break;
//        }
//        dbge("successfully open %s", fileNear);
//
//        readerFar = wavfile_reader_open(fileFar);
//        if(!readerFar){
//            dbge("fail to open %s", fileFar);
//            ret = -1;
//            break;
//        }
//        dbge("successfully open %s", fileFar);
//
//        writerOut = wavfile_writer_open_with_reader(fileOut, readerNear);
//        if(!writerOut){
//            dbge("fail to open %s", fileOut);
//            ret = -1;
//            break;
//        }
//        dbge("successfully open %s", fileOut);
//
//
//        int framebytes = nrOfSamples * sizeof(int16_t);
//        int frameCount = 0;
//
//        int samplerate = wavfile_reader_samplerate(readerFar);
//        int dropSamples = samplerate * dropMilliSeconds / 1000;
//        int dropFrames = (dropSamples + (nrOfSamples-1)) /nrOfSamples;
//        // dropFrames *= 2;
//        // dropFrames = 30;
//        dbgi("drop: samplerate=%d, dropSamples=%d, dropFrames=%d", samplerate, dropSamples, dropFrames);
//        while(frameCount < dropFrames){
//            ret = wavfile_reader_read(readerFar, far_buf, framebytes);
//            if(ret != framebytes){
//                dbgi("unexpectd reaching far-end file end");
//                ret = -1;
//                break;
//            }
//            ret = 0;
//            frameCount++;
//        }
//        if(ret) break;
//        dbgi("dropped frames %d", frameCount);
//
//        // run_aecm
//        // run_aec
//        // run_speex
//        ret =  run_aecm( readerNear
//        ,  readerFar
//        ,  writerOut
//        ,  sampFreq
//        ,  nrOfSamples
//        ,  msInSndCardBuf
//        ,  near_buf
//        ,  far_buf
//        ,  out_buf
//        ,  framebytes
//        ,  frameCount);
//
//
//        dbgi("processed frameCount = %d", frameCount);
//        dbgi("processed samples = %d", frameCount*nrOfSamples);
//        if(ret == 0){
//            dbgi("process OK");
//        }else{
//            dbgi("process fail !!!");
//        }
//
//    }while(0);
//
//
//
//    if(readerNear){
//        wavfile_reader_close(readerNear);
//        readerNear = NULL;
//    }
//
//    if(readerFar){
//        wavfile_reader_close(readerFar);
//        readerFar = NULL;
//    }
//
//    if(writerOut){
//        wavfile_writer_close(writerOut);
//        writerOut = NULL;
//    }
//
//    dbgi("bye!");
//    return 0;
//}

#define odbgv(FMT, ARGS...) //do{  printf("|%10s|V| " FMT, this->name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgd(FMT, ARGS...) do{  printf("|%11s|D| " FMT, this->name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%11s|I| " FMT, this->name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%11s|E| " FMT, this->name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)

using namespace webrtc;

static
inline void copy_int16_to_float(int16_t src[], float dst[], int num){
    for(int i = 0; i < num; i++){
        dst[i] = src[i];
    }
}

static
inline void copy_float_to_int16(float src[], int16_t dst[], int num){
    for(int i = 0; i < num; i++){
        dst[i] = src[i];
    }
}

struct XAudioConfig{
    int samplerate = 16000;
    int numChannels = 1;
};

struct XAudioFrame{
    int type = -1;
    int16_t * samples = NULL;
    int sampleBufferSize = 0;
    int samplesPerChannel = 0;
    int numChannels;
    int samplerate;
    int64_t timestamp = -1;
    virtual ~XAudioFrame(){}
};

struct XAudioFrameFixed : public XAudioFrame{
private:
    int16_t sampleBuffer[48000];
public:
    XAudioFrameFixed(){
        this->samples = sampleBuffer;
        this->sampleBufferSize = sizeof(sampleBuffer)/sizeof(sampleBuffer[0]);
    }
};

class XAudioFrameFactory{
protected:
    int frameSize_;
    std::list<XAudioFrame *> frames_;
    XAudioFrame * allocFrame(int bufferSize){
        XAudioFrame * frame = new XAudioFrame();
        if(bufferSize > 0){
            frame->samples = new int16_t[bufferSize];
        }
        frame->sampleBufferSize = bufferSize;
        return frame;
    }
    void freeFrame(XAudioFrame * frame){
        if(frame->samples){
            delete []frame->samples;
            frame->samples = NULL;
        }
        delete frame;
    }
public:
    XAudioFrameFactory(int frameSize=160, int numPreAlloc = 0):frameSize_(frameSize){
        for(int i = 0; i < numPreAlloc; ++i){
            frames_.push_back(allocFrame(frameSize));
        }
    }
    virtual ~XAudioFrameFactory(){
        for(auto frame : frames_){
            freeFrame(frame);
        }
        frames_.clear();
    }
    
    XAudioFrame * attainFrame(int bufferSize){
        if(frames_.size() > 0){
            XAudioFrame * frame = frames_.front();
            if(frame->sampleBufferSize >= bufferSize){
                frames_.pop_front();
                return frame;
            }else{
                // free it
                frames_.pop_front();
                freeFrame(frame);
            }
        }
        return allocFrame(bufferSize);
    }
    void returnFrame(XAudioFrame * frame){
        frames_.push_back(frame);
    }
    
};

class  XAudioReader{
public:
    virtual const XAudioConfig* getConfig() = 0;
    virtual XAudioFrame * read() = 0;
    virtual void close() = 0;
};


static const int BYTES_PER_SAMPLE = sizeof(int16_t);
static const int TYPE_DATA_BASE = 1000;
static const int TYPE_MAGIC = 501;
static const int TYPE_SAMPLERATE = 502;
static const int TYPE_CHANNELS = 503;
static const int minTlvHeaderSize = 4 + 4 + 8;

enum XAudioType {
    ANY = 0,
    PLAYING = 1010,
    MIC = 1011,
    AEC = 1012,
};

static
void makeObjName(const char * preName, std::string& name){
    static int instSeq = 0;
    char buf[64];
    sprintf(buf, "%s-%d", preName, ++instSeq);
    name = buf;
}

class XAudioTLVReader : public XAudioReader{
protected:
    
    XAudioFrameFactory * factory_;
    XAudioConfig config_;
    FILE * fp_ = NULL;
    int needType_ = 0;
    XAudioFrame * lastFrame_ = NULL;

    int readNext(XAudioFrame * &frame) {
        int ret = -1;
        XAudioFrame * next = NULL;
        do{
            frame = NULL;
            uint8_t header[minTlvHeaderSize];
            size_t bytes = fread(header, sizeof(char), minTlvHeaderSize, fp_);
            if(bytes != minTlvHeaderSize){
                ret = -1;
                break;
            }
            
            int type =  be_get_u32(header+0);
            int length = be_get_u32(header+4);
            int timestampH32 = be_get_u32(header+8);
            int timestampL32 = be_get_u32(header+12);
            int dataLength = length - sizeof(int64_t);
            int numSamples = (dataLength+BYTES_PER_SAMPLE-1)/BYTES_PER_SAMPLE;
            next = factory_->attainFrame(numSamples);
            if(dataLength > 0){
                bytes = fread(next->samples, sizeof(char), dataLength, fp_);
                if(bytes != dataLength){
                    ret = -1;
                    break;
                }
            }
            
            next->type = type;
            next->timestamp = ((int64_t)timestampH32 << 32) | timestampL32;
            next->samplesPerChannel = dataLength/BYTES_PER_SAMPLE/config_.numChannels;
            next->numChannels = config_.numChannels;
            next->samplerate = config_.samplerate;
            
            frame = next;
            next = NULL;
            ret = 0;
        }while(0);
        if(next){
            factory_->returnFrame(next);
            next = NULL;
        }
        return ret;
    }
    
    void readConfig(){
        XAudioFrame * frame = NULL;
        int samplerate = -1;
        int numChannels = -1;
        int ret = -1;
        while ((ret = readNext(frame)) == 0 ) {
            if(frame->type == TYPE_MAGIC){
                
            }else if(frame->type == TYPE_SAMPLERATE){
                samplerate = (int) frame->timestamp;
            }else if(frame->type == TYPE_CHANNELS){
                numChannels =(int) frame->timestamp;
            }else if(frame->type >= TYPE_DATA_BASE){
                lastFrame_ = frame;
                break;
            }
            factory_->returnFrame(frame);
            if(samplerate >= 0 && numChannels >= 0){
                config_.samplerate = samplerate;
                config_.numChannels = numChannels;
                return;
            }
        }
    }
    
public:
    XAudioTLVReader(XAudioFrameFactory * factory):factory_(factory){
    }
    
    virtual ~XAudioTLVReader(){
        close();
        if(lastFrame_){
            factory_->returnFrame(lastFrame_);
            lastFrame_ = NULL;
        }
    }
    
    int open(const std::string& filename, int needType = XAudioType::ANY){
        FILE * fp = NULL;
        int ret = -1;
        do{
            fp = fopen(filename.c_str(), "rb");
            if (!fp) {
                ret = -1;
                break;
            }
            fp_ = fp;
            fp = NULL;
            needType_ = needType;
            readConfig();
            ret = 0;
        }while(0);
        
        if(fp){
            fclose(fp);
            fp = NULL;
        }
        return ret;
    }
    
    virtual const XAudioConfig* getConfig() override{
        return &config_;
    }
    
    virtual XAudioFrame * read() override{
        XAudioFrame * frame = NULL;
        if(lastFrame_ != NULL){
            frame = lastFrame_;
            lastFrame_ = NULL;
        }else{
            readNext(frame);
        }
        while(frame != NULL){
            if(frame->type >= TYPE_DATA_BASE){
                if(needType_ == XAudioType::ANY){
                    return frame;
                }
                if(needType_ == frame->type){
                    return frame;
                }
            }
            factory_->returnFrame(frame);
            frame = NULL;
            readNext(frame);
        }
        return NULL;
    }
    
    virtual void close() override {
        if(fp_){
            fclose(fp_);
            fp_ = NULL;
        }
    }
};

class XAudioWaveReader : public XAudioReader{
protected:
    XAudioFrameFactory * factory_;
    XAudioConfig config_;
    wavfile_reader_t wavReader_ = NULL;
    int frameSizeInSamples_ = 0;
    
public:
    XAudioWaveReader(XAudioFrameFactory * factory):factory_(factory){
    }
    
    virtual ~XAudioWaveReader(){
        close();
    }
    
    int open(const std::string& filename, int frameSizeInMilliSeconds){
        int ret = -1;
        wavfile_reader_t reader = NULL;
        do{
            reader = wavfile_reader_open(filename.c_str());
            if (!reader) {
                ret = -1;
                break;
            }
            wavfileinfo info;
            wavfile_reader_info(reader, &info);
            config_.samplerate = info.samplerate;
            config_.numChannels = info.channels;
            frameSizeInSamples_ = info.samplerate * frameSizeInMilliSeconds/1000;
            wavReader_ = reader;
            reader = NULL;
            ret = 0;
        }while(0);
        
        if(reader){
            wavfile_reader_close(reader);
            reader = NULL;
        }
        return ret;
    }
    
    int skip(int numSamples){
        wavfile_reader_skip_short(wavReader_, numSamples);
        return 0;
    }
    
    virtual const XAudioConfig* getConfig() override{
        return &config_;
    }
    
    virtual XAudioFrame * read() override{
        XAudioFrame * next = factory_->attainFrame(frameSizeInSamples_);
        int numRead = wavfile_reader_read_short(wavReader_, next->samples, frameSizeInSamples_);
        if(numRead != frameSizeInSamples_){
            factory_->returnFrame(next);
            next = NULL;
        }else{
            next->type = XAudioType::ANY;
            next->timestamp = 0; // TODO:
            next->samplesPerChannel = frameSizeInSamples_/config_.numChannels;
            next->numChannels = config_.numChannels;
            next->samplerate = config_.samplerate;
        }
        return next;
    }
    
    virtual void close() override {
        if(wavReader_){
            wavfile_reader_close(wavReader_);
            wavReader_ = NULL;
        }
    }
};


class TLV2Wave{
protected:
    std::string name_;
public:
    TLV2Wave(){
        makeObjName("TLV2Wave", name_);
    }
    virtual ~TLV2Wave(){}
    int run(const char * tlvFilename, const char * outWaveFilename, XAudioType typ){
        XAudioFrameFactory * factory = new XAudioFrameFactory();
        XAudioTLVReader * reader = new XAudioTLVReader(factory);
        wavfile_writer_t wavwriter = NULL;
        int ret = -1;
        do{
            ret = reader->open(tlvFilename, typ);
            if(ret){
                odbge("fail to open [%s], ret=%d", tlvFilename, ret);
                break;
            }
            odbgi("opened tlv [%s]", tlvFilename);
            odbgi("numChannels=%d", reader->getConfig()->numChannels);
            odbgi("samplerate=%d", reader->getConfig()->samplerate);
            
            int numFrames = 0;
            XAudioFrame * frame = reader->read();
            if(frame){
                if(!wavwriter){
                    wavwriter = wavfile_writer_open(outWaveFilename, reader->getConfig()->numChannels, reader->getConfig()->samplerate);
                    if(!wavwriter){
                        ret = -1;
                        odbge("fail to open wave [%s]", outWaveFilename);
                        break;
                    }
                    odbgi("opened wave [%s]", outWaveFilename);
                }
            }
            while(frame){
                ret = wavfile_writer_write_short(wavwriter, frame->samples, frame->samplesPerChannel*frame->numChannels);
                factory->returnFrame(frame);
                frame = NULL;
                if(ret<=0){
                    ret = -1;
                    break;
                }
                ++numFrames;
                frame = reader->read();
            }
            odbgi("write frames %d", numFrames);
            if(numFrames == 0){
                odbge("fail to convert!");
                ret = -1;
                break;
            }
            ret = 0;
        }while(0);
        
        if(wavwriter){
            wavfile_writer_close(wavwriter);
            wavwriter = NULL;
        }
        
        if(reader){
            delete reader;
            reader = NULL;
        }
        
        if(factory){
            delete factory;
            factory = NULL;
        }
        return ret;
    }
    
    static int exec(const char * tlvFilename, const char * outWaveFilename, XAudioType typ=XAudioType::PLAYING);
};
int TLV2Wave::exec(const char *tlvFilename, const char *outWaveFilename, XAudioType typ){
    TLV2Wave obj;
    return obj.run(tlvFilename, outWaveFilename, typ);
}




class XAudioEchoCanceller {
protected:
    std::string name_;
public:
    XAudioEchoCanceller(const char * preName){
        makeObjName(preName, name_);
    }
    virtual ~XAudioEchoCanceller(){}
    virtual const std::string& name(){
        return name_;
    }
    virtual int preferFrameDuration(){
        return 10; // 10 milliseconds
    }
    virtual int open(int delay, int sampleRate, int channels, int frameSamplsPerChannel) = 0;
    virtual void close() = 0 ;
    virtual int processFar(short * farBuffer, int numSamples) = 0;
    virtual int processNear(short * nearBuffer, int numSamples, short * outBuffer) = 0;
};

class XAudioWebrtcAEC : public XAudioEchoCanceller{
    bool doNLP_;
    int delay_ = 0;
    void * aecInstance_ = NULL;
    float * inBufferF_ = NULL;
    float * outBufferF_ = NULL;
    
public:
    XAudioWebrtcAEC( bool doNLP = true):XAudioEchoCanceller("AEC"), doNLP_(doNLP){
    }
    
    virtual ~XAudioWebrtcAEC(){
        close();
    }
    
    virtual int open(int delay, int sampleRate, int channels, int frameSamplsPerChannel) override{
        if(aecInstance_){
            odbge("open already init");
            return -1;
        }
        delay_ = delay;
        odbgd("open frameSize=%d, sampleRate=%d, delay=%d, doNLP=%d", frameSamplsPerChannel, sampleRate, delay_, doNLP_);
        aecInstance_ = WebRtcAec_Create();
        WebRtcAec_Init(aecInstance_, sampleRate, 48000);
        AecConfig config;
        config.nlpMode = kAecNlpModerate;
        config.metricsMode = kAecTrue;
        config.skewMode = kAecFalse;
        config.delay_logging = kAecTrue;
        WebRtcAec_set_config(aecInstance_, config);
        
        
        inBufferF_ =  (float *) malloc(sizeof(float) * frameSamplsPerChannel);
        outBufferF_ = (float *) malloc(sizeof(float) * frameSamplsPerChannel);
        memset(inBufferF_,  0, frameSamplsPerChannel * sizeof(float));
        memset(outBufferF_, 0, frameSamplsPerChannel * sizeof(float));
        return 0;
    }
    
    virtual void close() override{
        if (aecInstance_) {
            WebRtcAec_Free(aecInstance_);
            aecInstance_ = NULL;
        }
        
        if(inBufferF_){
            free(inBufferF_);
            inBufferF_ = NULL;
        }
        
        if(outBufferF_){
            free(outBufferF_);
            outBufferF_ = NULL;
        }
    }
    
    int processFar(short * farBuffer, int numSamples) override{
        copy_int16_to_float(farBuffer, inBufferF_, numSamples);
        int32_t ret = WebRtcAec_BufferFarend(aecInstance_, inBufferF_, numSamples);
        odbgv("Far process: inst=%p, ref[0]=%+5d,ref[1]=%+5d,ref[2]=%+5d", aecInstance_, farBuffer[0], farBuffer[1], farBuffer[2]);
        return ret;
    }
    
    int processNear(short * nearBuffer, int numSamples, short * outBuffer) override{
        copy_int16_to_float(nearBuffer, inBufferF_, numSamples);
        odbgv("Mic process: inst=%p, mic[0]=%+5d,mic[1]=%+5d,mic[2]=%+5d =>", aecInstance_, nearBuffer[0], nearBuffer[1], nearBuffer[2]);
        const float* nearend = inBufferF_;
        int32_t ret = WebRtcAec_Process(aecInstance_, &nearend, 1, &outBufferF_, numSamples, delay_, 0);
        if(ret == 0){
            copy_float_to_int16(outBufferF_, outBuffer, numSamples);
            odbgv("Mic process: inst=%p, mic[0]=%+5d,mic[1]=%+5d,mic[2]=%+5d <=", aecInstance_, outBuffer[0], outBuffer[1], outBuffer[2]);
        }

        return ret;
    }
};

class XAudioWebrtcAECM : public XAudioEchoCanceller{
    void * aecInstance_ = NULL;
    int delay_ = 0;
    
public:
    XAudioWebrtcAECM():XAudioEchoCanceller("AECM"){
    }
    
    virtual ~XAudioWebrtcAECM(){
        close();
    }
    
    virtual int open(int delay, int sampleRate, int channels, int frameSamplsPerChannel) override{
        if(aecInstance_){
            odbge("open already init");
            return -1;
        }
        odbgd("open frameSize=%d, sampleRate=%d, delay=%d", frameSamplsPerChannel, sampleRate, delay);
        delay_ = delay;
        aecInstance_ = WebRtcAecm_Create();
        WebRtcAecm_Init(aecInstance_, sampleRate);
        AecmConfig aecmConfig = {.cngMode=0, .echoMode=3};
        int ret = WebRtcAecm_set_config(aecInstance_, aecmConfig);
        if(ret != 0){
            odbge("config fail ret=%d, config=[%d, %d]", ret, aecmConfig.cngMode, aecmConfig.echoMode);
            close();
            return -2;
        }
        odbgd("set config=[.cngMode=%d, .echoMode=%d]", aecmConfig.cngMode, aecmConfig.echoMode);
        
        return 0;
    }
    
    virtual void close() override{
        if (aecInstance_) {
            WebRtcAecm_Free(aecInstance_);
            aecInstance_ = NULL;
        }
    }
    
    int processFar(short * farBuffer, int numSamples) override{
        int ret = WebRtcAecm_BufferFarend(aecInstance_, farBuffer, numSamples);
        odbgv("AECM far process: inst=%p, ref[0]=%+5d,ref[1]=%+5d,ref[2]=%+5d", aecInstance_, farBuffer[0], farBuffer[1], farBuffer[2]);
        return ret;
    }
    
    int processNear(short * nearBuffer, int numSamples, short * outBuffer) override{
        odbgv("AECM mic process: inst=%p, mic[0]=%+5d,mic[1]=%+5d,mic[2]=%+5d =>", aecInstance_, nearBuffer[0], nearBuffer[1], nearBuffer[2]);
        int ret = WebRtcAecm_Process(aecInstance_, nearBuffer, NULL, outBuffer, numSamples, delay_);
        odbgv("AECM mic process: inst=%p, mic[0]=%+5d,mic[1]=%+5d,mic[2]=%+5d <=", aecInstance_, outBuffer[0], outBuffer[1], outBuffer[2]);
        return ret;
    }
};


class XAudioSpeexAEC : public XAudioEchoCanceller{
    bool preprocess_on_ = true;
    int delay_ = 0;
    SpeexPreprocessState *preprocess_state_ = NULL;
    SpeexEchoState *echo_state_ = NULL;
    int frameSamplsPerChannel_ = 0;
    short *farData_ = NULL;
    int farNum_ = 0;
    short *nearData_ = NULL;
    int nearNum_ = 0;
    
    void freeInternalBuf(){
        if(farData_){
            delete[] farData_;
            farData_ = NULL;
        }
        if(nearData_){
            delete[] nearData_;
            nearData_ = NULL;
        }
    }
public:
    XAudioSpeexAEC():XAudioEchoCanceller("SpeexAEC"){
    }
    
    virtual ~XAudioSpeexAEC(){
        close();
        freeInternalBuf();
    }
    
    virtual int preferFrameDuration() override{
        return 20; // 20 milliseconds
    }
    
    virtual int open(int delay, int sampleRate, int channels, int frameSamplsPerChannel) override{
        if(echo_state_){
            odbge("open already init");
            return -1;
        }
        
        int ret = 0;
        do {
            odbgd("open frameSize=%d, sampleRate=%d, delay=%d", frameSamplsPerChannel, sampleRate, delay);
            delay_ = delay;
            frameSamplsPerChannel_ = frameSamplsPerChannel;
            int filter_length = 4096; // 100*sampleRate/1000; // 100 ms
            echo_state_ = speex_echo_state_init(frameSamplsPerChannel, filter_length);
            if(!echo_state_){
                odbge("speex_echo_state_init fail");
                ret = -11;
                break;
            }
            
            int cfgInt = sampleRate;
            ret = speex_echo_ctl(echo_state_, SPEEX_ECHO_SET_SAMPLING_RATE, &cfgInt);
            if(ret != 0){
                odbge("speex_echo_ctl (SPEEX_ECHO_SET_SAMPLING_RATE) fail, ret=%d", ret);
                ret = -13;
                break;
            }
            
            if(preprocess_on_){
                preprocess_state_ = speex_preprocess_state_init(frameSamplsPerChannel, sampleRate);
                if(!preprocess_state_){
                    odbge("speex_preprocess_state_init fail");
                    ret = -21;
                    break;
                }
                odbgd("speex_preprocess_state_init success, obj=%p", preprocess_state_);
                
                
                ret = speex_preprocess_ctl(preprocess_state_, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state_);
                if(ret != 0){
                    odbge("speex_preprocess_ctl (SPEEX_PREPROCESS_SET_ECHO_STATE) fail, ret=%d", ret);
                    ret = -31;
                    break;
                }
                odbgd("speex_preprocess_ctl (SPEEX_PREPROCESS_SET_ECHO_STATE) success");
            }
            
            odbgd("speex init success, obj=%p, frame_size=%d, filter_length=%d, preprocess_on=%d", echo_state_, frameSamplsPerChannel, filter_length, preprocess_on_);
            
            freeInternalBuf();
            farData_ = new short[frameSamplsPerChannel];
            nearData_ = new short[frameSamplsPerChannel];
            
            ret = 0;
        } while (0);
        
        if(ret){
            close();
        }
        
        return ret;
    }
    
    virtual void close() override{
        if(preprocess_state_){
            odbgd("speex_preprocess_state_destroy aInst=%p", preprocess_state_);
            speex_preprocess_state_destroy(preprocess_state_);
            preprocess_state_ = NULL;
        }
        
        if(echo_state_){
            odbgd("speex_echo_state_destroy aInst=%p", echo_state_);
            speex_echo_state_destroy(echo_state_);
            echo_state_ = NULL;
        }
    }
    
    int doAEC(short * farBuffer, short * nearBuffer, short * outBuffer){
//        speex_echo_playback(echo_state_, farBuffer);
//        if(preprocess_state_){
//            speex_preprocess_run(preprocess_state_, nearBuffer);
//        }
//        speex_echo_capture(echo_state_, nearBuffer, outBuffer);
        
        speex_echo_cancellation(echo_state_, nearBuffer, farBuffer, outBuffer);
        if(preprocess_state_){
            speex_preprocess_run(preprocess_state_, outBuffer);
        }
        return 0;
    }
    
    int processFar(short * farBuffer, int numSamples) override{
        if(numSamples != frameSamplsPerChannel_){
            return -1;
        }
        memcpy(farData_, farBuffer, numSamples*sizeof(short));
        farNum_ = numSamples;
        return 0;
    }
    
    int processNear(short * nearBuffer, int numSamples, short * outBuffer) override{
        if(numSamples != frameSamplsPerChannel_){
            return -1;
        }
        if(farNum_ > 0){
            doAEC(farData_, nearBuffer, outBuffer);
            farNum_ = 0;
        }else{
            memcpy(outBuffer, nearBuffer, numSamples*sizeof(short));
        }
        return 0;
    }
};

template <class T, class SizeT=int>
class BucketBuffer {
    SizeT bucketSize_ = 0;
    SizeT bufSize_ = 0;
    T * buf_ = NULL;
    SizeT bufLength_ = 0;
    SizeT bufPos_ = 0;
public:
    BucketBuffer(){}
    virtual ~BucketBuffer(){
        close();
    }
    
    void open(SizeT bucketSize){
        close();
        bucketSize_ = bucketSize;
        bufSize_ = bucketSize_ * 2;
        if(bucketSize_ > 0){
            buf_ = new T[bufSize_];
        }
    }
    
    void close(){
        if(buf_){
            delete[] buf_;
            buf_ = NULL;
        }
    }
    
    void push(T * data, SizeT length){
        if(bufPos_ > 0){
            if(bufPos_ < bufLength_){
                SizeT numRemains = (bufLength_ - bufPos_);
                memmove(buf_+0, buf_+bufPos_, numRemains);
                bufPos_ = 0;
                bufLength_ = numRemains;
            }else{
                bufPos_ = 0;
                bufLength_ = 0;
            }
        }
        if((bufLength_+length) <= bufSize_){
            memcpy(buf_+bufLength_, data+0, length*sizeof(T));
            bufLength_ += bufLength_;
        }else if(length >= bufSize_){
            memcpy(buf_+0, data+length-bufSize_, bufSize_*sizeof(T));
            bufLength_ = bufSize_;
        }else{
            SizeT numRemains = (bufSize_ - length);
            memmove(buf_+0, buf_+bufLength_-numRemains, numRemains);
            bufLength_ = numRemains;
            
            memcpy(buf_+bufLength_, data+0, length);
            bufLength_ += length;
        }
    }
    
    bool full(){
        SizeT numRemains = (bufLength_ - bufPos_);
        return (numRemains >= bucketSize_);
    }
    
    T * pull(){
        if(full()){
            T * data = buf_+bufPos_;
            bufPos_ += bucketSize_;
            return data;
        }
        return NULL;
    }
    

};


class XAudioSabineSpeexAEC : public XAudioEchoCanceller{
    int delay_ = 0;
    SpeexAec * aecInst_ = NULL;
    int frameSamplsPerChannel_ = 0;
    short *farData_ = NULL;
    int farNum_ = 0;
    short *nearData_ = NULL;
    int nearNum_ = 0;
    
    void freeInternalBuf(){
        if(farData_){
            delete[] farData_;
            farData_ = NULL;
        }
        if(nearData_){
            delete[] nearData_;
            nearData_ = NULL;
        }
    }
public:
    XAudioSabineSpeexAEC():XAudioEchoCanceller("SabSpxAEC"){
    }
    
    virtual ~XAudioSabineSpeexAEC(){
        close();
        freeInternalBuf();
    }
    
    virtual int preferFrameDuration() override{
        return 20; // 20 milliseconds
    }
    
    virtual int open(int delay, int sampleRate, int channels, int frameSamplsPerChannel) override{
        if(aecInst_){
            odbge("open already init");
            return -1;
        }
        
        int ret = 0;
        do {
            odbgd("open frameSize=%d, sampleRate=%d, delay=%d", frameSamplsPerChannel, sampleRate, delay);
            delay_ = delay;
            frameSamplsPerChannel_ = frameSamplsPerChannel;
            aecInst_ = SabineAecOpen(sampleRate, frameSamplsPerChannel_);
            if(!aecInst_){
                odbge("SabineAecOpen fail");
                ret = -11;
                break;
            }
            odbgd("SabineAecOpen success, obj=%p", aecInst_);
            
            freeInternalBuf();
            farData_ = new short[frameSamplsPerChannel];
            nearData_ = new short[frameSamplsPerChannel];
            
            ret = 0;
        } while (0);
        
        if(ret){
            close();
        }
        
        return ret;
    }
    
    virtual void close() override{
        if(aecInst_){
            odbgd("SabineAecClose aInst=%p", aecInst_);
            SabineAecClose(aecInst_);
            aecInst_ = NULL;
        }
    }
    
    int doAEC(short * farBuffer, short * nearBuffer, short * outBuffer){
        SabineAecExecute(aecInst_, nearBuffer, farData_);
        memcpy(outBuffer, nearBuffer, frameSamplsPerChannel_ * BYTES_PER_SAMPLE);
        return 0;
    }
    
    int processFar(short * farBuffer, int numSamples) override{
        if(numSamples != frameSamplsPerChannel_){
            return -1;
        }
        memcpy(farData_, farBuffer, numSamples*sizeof(short));
        farNum_ = numSamples;
        return 0;
    }
    
    int processNear(short * nearBuffer, int numSamples, short * outBuffer) override{
        if(numSamples != frameSamplsPerChannel_){
            return -1;
        }
        if(farNum_ > 0){
            doAEC(farData_, nearBuffer, outBuffer);
            farNum_ = 0;
        }else{
            memcpy(outBuffer, nearBuffer, numSamples*sizeof(short));
        }
        return 0;
    }
};

//class XAudioSabineSpeexAEC1 : public XAudioEchoCanceller {
//    int delay_ = 0;
//    SpeexAec * aecInst_ = NULL;
//    int frameSamplsPerChannel_ = 0;
//    BucketBuffer<short> farBucket_;
//    BucketBuffer<short> nearBucket_;
//
//public:
//    XAudioSabineSpeexAEC`():XAudioEchoCanceller("SabSpxAEC"){
//    }
//
//    virtual ~XAudioSabineSpeexAEC`(){
//        close();
//    }
//
//
//    virtual int open(int delay, int sampleRate, int channels, int frameSamplsPerChannel) override{
//        if(aecInst_){
//            odbge("open already init");
//            return -1;
//        }
//
//        int ret = 0;
//        do {
//            odbgd("open frameSize=%d, sampleRate=%d, delay=%d", frameSamplsPerChannel, sampleRate, delay);
//            delay_ = delay;
//            frameSamplsPerChannel_ = frameSamplsPerChannel;
//            aecInst_ = SabineAecOpen(sampleRate);
//            if(!aecInst_){
//                odbge("SabineAecOpen fail");
//                ret = -11;
//                break;
//            }
//            odbgd("SabineAecOpen success, obj=%p", aecInst_);
//
//            farBucket_.open(frameSamplsPerChannel * 2);
//            nearBucket_.open(frameSamplsPerChannel * 2);
//
//            ret = 0;
//        } while (0);
//
//        if(ret){
//            close();
//        }
//
//        return ret;
//    }
//
//    virtual void close() override{
//        if(aecInst_){
//            odbgd("SabineAecClose aInst=%p", aecInst_);
//            SabineAecClose(aecInst_);
//            aecInst_ = NULL;
//        }
//        farBucket_.close();
//        nearBucket_.close();
//    }
//
//    int doAEC(short * farBuffer, short * nearBuffer, short * outBuffer){
//        SabineAecExecute(aecInst_, nearBuffer, farData_);
//        memcpy(outBuffer, nearBuffer, frameSamplsPerChannel_ * BYTES_PER_SAMPLE);
//        return 0;
//    }
//
//    int processFar(short * farBuffer, int numSamples) override{
//        if(numSamples != frameSamplsPerChannel_){
//            return -1;
//        }
//        farBucket_.push(farBuffer, numSamples);
//        return 0;
//    }
//
//    int processNear(short * nearBuffer, int numSamples, short * outBuffer) override{
//        if(numSamples != frameSamplsPerChannel_){
//            return -1;
//        }
//        nearBucket_.push(nearBuffer, numSamples);
//        if(nearBucket_.full() && farBucket_.full()){
//            short * mic = nearBucket_.pull();
//            SabineAecExecute(aecInst_, mic, farBucket_.pull());
//            memcpy(outBuffer, nearBuffer, frameSamplsPerChannel_ * BYTES_PER_SAMPLE);
//        }
//
//        if(farNum_ > 0){
//            doAEC(farData_, nearBuffer, outBuffer);
//            farNum_ = 0;
//        }else{
//            memcpy(outBuffer, nearBuffer, numSamples*sizeof(short));
//        }
//        return 0;
//    }
//};


//static
//int run_speex(wavfile_reader_t readerNear
//        , wavfile_reader_t readerFar
//        , wavfile_writer_t writerOut
//        , int32_t sampFreq
//        , int nrOfSamples
//        , int msInSndCardBuf
//        , int16_t near_buf[]
//        , int16_t far_buf[]
//        , int16_t out_buf[]
//        , int framebytes
//        , int& frameCount){
//
//    int ret = 0;
//
//    SpeexPreprocessState *preprocess_state = NULL;
//    SpeexEchoState *echo_state = NULL;
//    int  preprocess_on = 1;
//
//    nrOfSamples = 2 * sampFreq/100; // 20 ms
//    framebytes = nrOfSamples * 2;
//    int frame_size = nrOfSamples;
//    int filter_length =  sampFreq/10/2; // 100 ms
//
//    dbgi("use speex echo (preprocess_on=%d)", preprocess_on);
//    dbgi("frame_size=%d", frame_size);
//    dbgi("filter_length=%d", filter_length);
//
//    do{
//
//        echo_state = speex_echo_state_init(frame_size, filter_length);
//        if(!echo_state){
//            dbge("speex_echo_state_init fail");
//            break;
//        }
//        dbgi("speex_echo_state_init success, obj=%p", echo_state);
//
//        int sampleRate = sampFreq;
//        speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
//
//        if(preprocess_on){
//            preprocess_state = speex_preprocess_state_init(frame_size, sampFreq);
//            if(!preprocess_state){
//                dbge("speex_preprocess_state_init fail");
//                break;
//            }
//            dbgi("speex_preprocess_state_init success, obj=%p", preprocess_state);
//
//
//            ret = speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state);
//            if(ret != 0){
//                dbge("speex_preprocess_ctl (SPEEX_PREPROCESS_SET_ECHO_STATE) fail, ret=%d", ret);
//                break;
//            }
//            dbge("speex_preprocess_ctl (SPEEX_PREPROCESS_SET_ECHO_STATE) success");
//        }
//
//
//
//        frameCount = 0;
//        while(1){
//            ret = wavfile_reader_read(readerFar, far_buf, framebytes);
//            if(ret != framebytes){
//                dbgi("reach far-end file end");
//                ret = 0;
//                break;
//            }
//
//            ret = wavfile_reader_read(readerNear, near_buf, framebytes);
//            if(ret != framebytes){
//                dbgi("reach near-end file end");
//                ret = 0;
//                break;
//            }
//
//            // if(preprocess_state){
//            //     speex_preprocess_run(preprocess_state, near_buf);
//            // }
//            speex_preprocess_run(preprocess_state, near_buf);
//            speex_echo_playback(echo_state, far_buf);
//            speex_echo_capture(echo_state, near_buf, out_buf);
//
//
//            // speex_echo_cancellation(echo_state, far_buf, near_buf, out_buf);
//            // speex_preprocess_run(preprocess_state, out_buf);
//
//
//
//            //memcpy(out_buf, near_buf, framebytes);
//            ret = wavfile_writer_write(writerOut, out_buf, framebytes);
//            if(ret < 0){
//                dbge("wavfile_writer_write fail ret=%d", ret);
//                break;
//            }
//
//            ret = 0;
//            frameCount++;
//        }
//
//    }while(0);
//
//    if(preprocess_state){
//        dbgi("speex_preprocess_state_destroy aInst=%p", preprocess_state);
//        speex_preprocess_state_destroy(preprocess_state);
//        preprocess_state = NULL;
//    }
//
//    if(echo_state){
//        dbgi("speex_echo_state_destroy aInst=%p", echo_state);
//        speex_echo_state_destroy(echo_state);
//        echo_state = NULL;
//    }
//
//    return ret;
//}

class TLVFileAEC{
protected:
    std::string name_;
public:
    TLVFileAEC(){
        makeObjName("TLV-AEC", name_);
    }
    virtual ~TLVFileAEC(){}
    
    int run(const char * tlvFilename, const char * outWaveFilename, XAudioEchoCanceller * aec){
        XAudioFrameFactory * factory = new XAudioFrameFactory();
        XAudioTLVReader * reader = new XAudioTLVReader(factory);
        wavfile_writer_t wavwriter = NULL;
        int delay = 115; // 120, 134, 260
        int ret = -1;
        do{
            ret = reader->open(tlvFilename, XAudioType::ANY);
            if(ret){
                odbge("fail to open [%s], ret=%d", tlvFilename, ret);
                break;
            }
            odbgi("opened tlv [%s]", tlvFilename);
            odbgi("numChannels=%d", reader->getConfig()->numChannels);
            odbgi("samplerate=%d", reader->getConfig()->samplerate);
            
            wavwriter = wavfile_writer_open(outWaveFilename, reader->getConfig()->numChannels, reader->getConfig()->samplerate);
            if(!wavwriter){
                ret = -1;
                odbge("fail to open wave [%s]", outWaveFilename);
                break;
            }
            odbgi("opened wave [%s]", outWaveFilename);
            
            odbgi("system delay %d", delay);
            int aecDelay = delay;
            int frameSamplsPerChannel = 10 * reader->getConfig()->samplerate/1000;
            ret = aec->open(aecDelay, reader->getConfig()->samplerate, reader->getConfig()->numChannels, frameSamplsPerChannel );
            if(ret){
                odbge("fail to open aec [%s]", aec->name().c_str());
                break;
            }
            odbgi("opened aec [%s], delay %d", aec->name().c_str(), aecDelay);
            
            int numFrames = 0;
            short outbuffer[reader->getConfig()->samplerate];
            XAudioFrame * frame = reader->read();
            while(frame){
                ret = 0;
                int numSamples = frame->samplesPerChannel*frame->numChannels;
                if(frame->type == XAudioType::PLAYING){
                    ret = aec->processFar(frame->samples, numSamples);
                    if(ret){
                        odbge("fail to aec processFar, error=%d", ret);
                    }
                }else if(frame->type == XAudioType::MIC){
                    ret = aec->processNear(frame->samples, numSamples, outbuffer);
                    if(ret){
                        odbge("fail to aec processNear, error=%d", ret);
                    }else{
                        ret = wavfile_writer_write_short(wavwriter, outbuffer, numSamples);
                        if(ret<=0){
                            ret = -1;
                        }else{
                            ret = 0;
                            ++numFrames;
                        }
                    }
                }
                frame->samplesPerChannel = 0;
                factory->returnFrame(frame);
                frame = NULL;
                if(ret){
                    break;
                }
                frame = reader->read();
            }
            odbgi("write frames %d", numFrames);
            ret = numFrames;
        }while(0);
        
        if(aec){
            delete aec;
            aec = NULL;
        }
        
        if(wavwriter){
            wavfile_writer_close(wavwriter);
            wavwriter = NULL;
        }
        
        if(reader){
            delete reader;
            reader = NULL;
        }
        
        if(factory){
            delete factory;
            factory = NULL;
        }
        return ret;
    }
    
    static int exec(const char * tlvFilename, const char * outWaveFilename, XAudioEchoCanceller * aec);
};

int TLVFileAEC::exec(const char *tlvFilename, const char *outWaveFilename, XAudioEchoCanceller * aec){
    TLVFileAEC obj;
    return obj.run(tlvFilename, outWaveFilename, aec);
}

class WavFileAEC{
protected:
    std::string name_;
public:
    WavFileAEC(){
        makeObjName("WAV-AEC", name_);
    }
    virtual ~WavFileAEC(){}
    
    int run(const char * farWaveFilename, const char * nearWaveFilename, const char * outWaveFilename, XAudioEchoCanceller * aec, int delay){
        XAudioFrameFactory * factory = new XAudioFrameFactory();
        XAudioWaveReader * farReader = new XAudioWaveReader(factory);
        XAudioWaveReader * nearReader = new XAudioWaveReader(factory);
        wavfile_writer_t wavwriter = NULL;
//        int delay = 120; // 120, 134, 260
        int ret = -1;
        do{
            int frameDuration = aec->preferFrameDuration(); // 10;
            ret = farReader->open(farWaveFilename, frameDuration);
            if(ret){
                odbge("fail to open [%s], ret=%d", farWaveFilename, ret);
                break;
            }
            odbgi("opened wave [%s]", farWaveFilename);
            odbgi(" numChannels=%d", farReader->getConfig()->numChannels);
            odbgi(" samplerate=%d", farReader->getConfig()->samplerate);
            
            ret = nearReader->open(nearWaveFilename, frameDuration);
            if(ret){
                odbge("fail to open [%s], ret=%d", nearWaveFilename, ret);
                break;
            }
            odbgi("opened wave [%s]", nearWaveFilename);
            odbgi(" numChannels=%d", nearReader->getConfig()->numChannels);
            odbgi(" samplerate=%d", nearReader->getConfig()->samplerate);
            
            if(farReader->getConfig()->numChannels != nearReader->getConfig()->numChannels
               || farReader->getConfig()->samplerate != nearReader->getConfig()->samplerate){
                odbge("inconsist audio config!");
                ret = -1;
                break;
            }
            
            wavwriter = wavfile_writer_open(outWaveFilename, farReader->getConfig()->numChannels, farReader->getConfig()->samplerate);
            if(!wavwriter){
                ret = -1;
                odbge("fail to open wave [%s]", outWaveFilename);
                break;
            }
            odbgi("opened wave [%s]", outWaveFilename);
            
            odbgi("system delay %d, frame-duration %d", delay, frameDuration);
            
            int skipSamples = farReader->getConfig()->samplerate * std::abs(delay) / 1000;
            if(delay > 0){
                odbgi("skip far %d samples, %d milliseconds", skipSamples, delay);
                farReader->skip(skipSamples);
            }else if(delay < 0){
                odbgi("skip near %d samples, %d milliseconds", skipSamples, -delay);
                nearReader->skip(skipSamples);
            }
            
            int aecDelay = 0;
            int frameSamplsPerChannel = frameDuration * farReader->getConfig()->samplerate / 1000;
            ret = aec->open(aecDelay, farReader->getConfig()->samplerate, farReader->getConfig()->numChannels, frameSamplsPerChannel );
            if(ret){
                odbge("fail to open aec [%s]", aec->name().c_str());
                break;
            }
            odbgi("opened aec [%s], delay %d", aec->name().c_str(), aecDelay);

            
            int numFrames = 0;
            short outbuffer[farReader->getConfig()->samplerate];
            XAudioFrame * farFrame = farReader->read();
            XAudioFrame * nearFrame = nearReader->read();
            while(farFrame && nearFrame){
                ret = 0;
                int numSamples = farFrame->samplesPerChannel * farFrame->numChannels;

                ret = aec->processFar(farFrame->samples, numSamples);
                if(ret){
                    odbge("fail to aec processFar, error=%d", ret);
                    break;
                }
                
                ret = aec->processNear(nearFrame->samples, numSamples, outbuffer);
                if(ret){
                    odbge("fail to aec processNear, error=%d", ret);
                }else{
                    ret = wavfile_writer_write_short(wavwriter, outbuffer, numSamples);
                    if(ret<=0){
                        ret = -1;
                    }else{
                        ret = 0;
                        ++numFrames;
                    }
                }
                if(ret){
                    break;
                }
                

                factory->returnFrame(farFrame);
                factory->returnFrame(nearFrame);
                farFrame = farReader->read();
                nearFrame = nearReader->read();
            }
            if(farFrame){
                factory->returnFrame(farFrame);
                farFrame = NULL;
            }
            if(nearFrame){
                factory->returnFrame(nearFrame);
                nearFrame = NULL;
            }
            
            odbgi("write frames %d", numFrames);
            ret = numFrames;
        }while(0);
        
        if(aec){
            delete aec;
            aec = NULL;
        }
        
        if(wavwriter){
            wavfile_writer_close(wavwriter);
            wavwriter = NULL;
        }
        
        if(farReader){
            delete farReader;
            farReader = NULL;
        }
        
        if(nearReader){
            delete nearReader;
            nearReader = NULL;
        }
        
        if(factory){
            delete factory;
            factory = NULL;
        }
        return ret;
    }
    
    static int exec(const char * farWaveFilename, const char * nearWaveFilename, const char * outWaveFilename, XAudioEchoCanceller * aec, int delay);
};

int WavFileAEC::exec(const char * farWaveFilename, const char * nearWaveFilename, const char * outWaveFilename, XAudioEchoCanceller * aec, int delay){
    WavFileAEC obj;
    return obj.run(farWaveFilename, nearWaveFilename, outWaveFilename, aec, delay);
}


int main(int argc, char** argv){
    const char * tlvFilename = "/Users/simon/Downloads/VAECDemo/src-aec.tlv";
    const char * outWaveFilename = "/Users/simon/Downloads/VAECDemo/macout.wav";
    int ret = 0;
//    ret = TLVFileAEC::exec(tlvFilename, outWaveFilename, new XAudioWebrtcAEC());
//    ret = TLVFileAEC::exec(tlvFilename, outWaveFilename, new XAudioWebrtcAECM());
    
    
//    const char * farWaveFilename = "/Users/simon/Downloads/VAECDemo/aec-far.wav";
//    const char * nearWaveFilename = "/Users/simon/Downloads/VAECDemo/aec-near.wav";
//    ret = TLV2Wave::exec(tlvFilename, farWaveFilename, XAudioType::PLAYING);
//    ret = TLV2Wave::exec(tlvFilename, nearWaveFilename, XAudioType::MIC);
//    ret = WavFileAEC::exec(farWaveFilename, nearWaveFilename, outWaveFilename, new XAudioWebrtcAEC());
//    ret = WavFileAEC::exec(farWaveFilename, nearWaveFilename, outWaveFilename, new XAudioWebrtcAECM());
//    ret = WavFileAEC::exec(farWaveFilename, nearWaveFilename, outWaveFilename, new XAudioSpeexAEC());
//    ret = WavFileAEC::exec(farWaveFilename, nearWaveFilename, outWaveFilename, new XAudioSabineSpeexAEC());

    const char * farWaveFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/src/sabine_speex/hua_ref.wav";
    const char * nearWaveFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/src/sabine_speex/hua_mic.wav";
    ret = WavFileAEC::exec(farWaveFilename, nearWaveFilename, outWaveFilename, new XAudioWebrtcAECM(), 0);
    
    return ret;
}
