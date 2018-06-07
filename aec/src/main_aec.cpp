#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h> // SIGINT
#include <list>
#include <string>
#include <cmath>

#include "webrtc/modules/audio_processing/aecm/echo_control_mobile.h"
#include "webrtc/modules/audio_processing/aec/echo_cancellation.h"

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include "xcutil.h"
#include "xwavfile.h"
#include "sabine_aec.h"


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




#define odbgv(FMT, ARGS...) //do{  printf("|%10s|V| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgd(FMT, ARGS...) do{  printf("|%11s|D| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%11s|I| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%11s|E| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)

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
    
    void setFrameDuration(int frameMilliSeconds){
        frameSizeInSamples_ = config_.samplerate * frameMilliSeconds/1000;
    }
    
    int open(const std::string& filename, int frameSizeInMilliSeconds = 0){
        int ret = -1;
        wavfile_reader_t reader = NULL;
        do{
            if(frameSizeInMilliSeconds == 0){
                frameSizeInMilliSeconds  =10;
            }
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
    
    void printLevel(const char * name, AecLevel& level){
        odbgi("%s: inst=%3d, avr=%3d, max=%3d, min=%3d", name, level.instant, level.average, level.max, level.min);
    }
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
            AecMetrics metrics;
            int ret = WebRtcAec_GetMetrics(aecInstance_, &metrics);
            if(ret == 0){
                odbgi("metrics -----");
                printLevel("  rerl", metrics.rerl);
                printLevel("  erl", metrics.erl);
                printLevel("  erle", metrics.erle);
                printLevel("  nlp", metrics.aNlp);
                int median, std;
                float fraction_poor_delays;
                WebRtcAec_GetDelayMetrics(aecInstance_, &median, &std, & fraction_poor_delays);
                odbgi("  delay: median=%3d, std=%3d, poort_delay=%3.2f%%", median, std, fraction_poor_delays*100.0);
                
            }else{
                odbge("WebRtcAec_GetMetrics fail with ret=%d", ret);
            }
            
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
    bool preprocess_on_ = false;
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
    XAudioSpeexAEC(bool preprocess):XAudioEchoCanceller("SpeexAEC"), preprocess_on_(preprocess){
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
//            memset(farData_, 0, frameSamplsPerChannel*sizeof(short));
//            memset(nearData_, 0, frameSamplsPerChannel*sizeof(short));
//            speex_echo_playback(echo_state_, farData_);
//            speex_echo_capture(echo_state_, nearData_, nearData_);
            
            
            
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
    
    int doAEC2(short * farBuffer, short * nearBuffer, short * outBuffer){
        speex_echo_playback(echo_state_, farBuffer);
        if(preprocess_state_){
            speex_preprocess_run(preprocess_state_, nearBuffer);
        }
        speex_echo_capture(echo_state_, nearBuffer, outBuffer);
//        if(preprocess_state_){
//            speex_preprocess_run(preprocess_state_, outBuffer);
//        }
        return 0;
    }
    
    int doAEC1(short * farBuffer, short * nearBuffer, short * outBuffer){
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
            doAEC1(farData_, nearBuffer, outBuffer);
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


class TLVFileAEC{
protected:
    std::string name_;
public:
    TLVFileAEC(){
        makeObjName("TLV-AEC", name_);
    }
    virtual ~TLVFileAEC(){}
    
    int run(const char * tlvFilename, const char * outWaveFilename, XAudioEchoCanceller * aec, int delay){
        XAudioFrameFactory * factory = new XAudioFrameFactory();
        XAudioTLVReader * reader = new XAudioTLVReader(factory);
        wavfile_writer_t wavwriter = NULL;
//        int delay = 115; // 120, 134, 260
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
    
    static int exec(const char * tlvFilename, const char * outWaveFilename, XAudioEchoCanceller * aec, int delay);
};

int TLVFileAEC::exec(const char *tlvFilename, const char *outWaveFilename, XAudioEchoCanceller * aec, int delay){
    TLVFileAEC obj;
    return obj.run(tlvFilename, outWaveFilename, aec, delay);
}

class AECSource{
public:
    virtual ~AECSource(){}
    virtual void close() = 0;
    virtual void setFrameDuration(int frameMilliSeconds) = 0;
    virtual int getSamplerate() = 0;
    virtual int getChannels() = 0;
    virtual void readNext(XAudioFrame * &farFrame, XAudioFrame * &nearFrame) = 0;
};


class WaveAECSource : public AECSource{
protected:
    std::string name_;
    XAudioFrameFactory * frameFactory_ = NULL;
    wavfile_reader_t wavFarReader_ = NULL;
    wavfile_reader_t wavNearReader_ = NULL;
    wavfileinfo farInfo_;
    wavfileinfo nearInfo_;
    int frameSizeInSamples_ = 0;
    
public:
    WaveAECSource(XAudioFrameFactory * frameFactory)
    : frameFactory_(frameFactory){
        makeObjName("WAVE-SRC", name_);
    }
    virtual ~WaveAECSource(){
        close();
    }
    
    int open(const char * farWaveFilename, const char * nearWaveFilename, int skipDuration = 0){
        int ret = -1;
        do{
            
            wavFarReader_ = wavfile_reader_open(farWaveFilename);
            if (!wavFarReader_) {
                ret = -1;
                odbge("fail to open [%s], ret=%d", farWaveFilename, ret);
                break;
            }
            wavfile_reader_info(wavFarReader_, &farInfo_);
            odbgi("opened wave [%s]", farWaveFilename);
            
            wavNearReader_ = wavfile_reader_open(nearWaveFilename);
            if (!wavNearReader_) {
                ret = -1;
                odbge("fail to open [%s], ret=%d", nearWaveFilename, ret);
                break;
            }
            wavfile_reader_info(wavNearReader_, &nearInfo_);
            odbgi("opened wave [%s]", nearWaveFilename);
            
            if(farInfo_.channels != nearInfo_.channels
               || farInfo_.samplerate != nearInfo_.samplerate){
                odbge("inconsist audio config!");
                ret = -1;
                break;
            }
            
            if(frameSizeInSamples_ == 0){
                // never set before, use default
                frameSizeInSamples_ = 10*farInfo_.samplerate/1000;
            }
            
            int skipSamples = farInfo_.samplerate * std::abs(skipDuration) / 1000;
            if(skipDuration > 0){
                odbgi("skip far %d samples, %d milliseconds", skipSamples, skipDuration);
                wavfile_reader_skip_short(wavFarReader_, skipSamples);
            }else if(skipDuration < 0){
                odbgi("skip near %d samples, %d milliseconds", skipSamples, -skipDuration);
                wavfile_reader_skip_short(wavNearReader_, skipSamples);
            }
            
            
            ret = 0;
        }while(0);
        if(ret){
            close();
        }
        return ret;
    }
    
    virtual void close() override{
        if(wavNearReader_){
            wavfile_reader_close(wavNearReader_);
            wavNearReader_ = NULL;
        }
        if(wavFarReader_){
            wavfile_reader_close(wavFarReader_);
            wavFarReader_ = NULL;
        }
    }
    
    virtual void setFrameDuration(int frameMilliSeconds) override{
        frameSizeInSamples_ = frameMilliSeconds*farInfo_.samplerate/1000;
    }
    
    virtual int getSamplerate()override{
        return farInfo_.samplerate;
    }
    virtual int getChannels()override{
        return 1;
    }
    
    XAudioFrame * read(wavfile_reader_t wavReader_, wavfileinfo &info, int numSamples, XAudioType typ){
        XAudioFrame * next = frameFactory_->attainFrame(numSamples);
        int numRead = wavfile_reader_read_short(wavReader_, next->samples, numSamples);
        if(numRead != numSamples){
            frameFactory_->returnFrame(next);
            next = NULL;
        }else{
            next->type = typ;
            next->timestamp = 0; // TODO:
            next->samplesPerChannel = numSamples/info.channels;
            next->numChannels = info.channels;
            next->samplerate = info.samplerate;
        }
        return next;
    }
    
    virtual void readNext(XAudioFrame * &farFrame, XAudioFrame * &nearFrame) override{
        farFrame = this->read(wavFarReader_, farInfo_, frameSizeInSamples_, XAudioType::PLAYING);
        nearFrame = this->read(wavNearReader_, nearInfo_, frameSizeInSamples_, XAudioType::MIC);
    }
    
};

class SingleWaveAECSource : public AECSource{
protected:
    std::string name_;
    XAudioFrameFactory * frameFactory_ = NULL;
    wavfile_reader_t wavFarReader_ = NULL;
    wavfileinfo farInfo_;
    int frameSizeInSamples_ = 0;
    short * sampleBuf_ = NULL;
    int sampleBufSize_ = 0;
    int numSample2Read_ = 0;
    
    void reallocInternalBuf(int samplePerChannel){
        int newSize = samplePerChannel * farInfo_.channels;
        numSample2Read_ = newSize;
        if(newSize == 0){
            if(sampleBuf_){
                delete[] sampleBuf_;
                sampleBuf_ = NULL;
            }
            sampleBufSize_ = 0;
        }
        else if(sampleBufSize_ < newSize){
            if(sampleBuf_){
                delete[] sampleBuf_;
                sampleBuf_ = NULL;
            }
            sampleBuf_ = new short[newSize];
            sampleBufSize_ = newSize;
        }
        
    }
public:
    SingleWaveAECSource(XAudioFrameFactory * frameFactory)
    : frameFactory_(frameFactory){
        makeObjName("WAVE-SRC", name_);
    }
    virtual ~SingleWaveAECSource(){
        close();
    }
    
    int open(const char * farWaveFilename, int skipDuration = 0){
        int ret = -1;
        do{
            
            wavFarReader_ = wavfile_reader_open(farWaveFilename);
            if (!wavFarReader_) {
                ret = -11;
                odbge("fail to open [%s], ret=%d", farWaveFilename, ret);
                break;
            }
            wavfile_reader_info(wavFarReader_, &farInfo_);
            if(farInfo_.channels < 2){
                ret = -21;
                odbge("require channels >= 2, file=[%s]", farWaveFilename);
                break;
            }
            odbgi("opened wave [%s]", farWaveFilename);
            
            if(frameSizeInSamples_ == 0){
                // never set before, use default
                frameSizeInSamples_ = 10*farInfo_.samplerate/1000;
                reallocInternalBuf(frameSizeInSamples_);
            }
            
            
            ret = 0;
        }while(0);
        if(ret){
            close();
        }
        return ret;
    }
    
    virtual void close() override{
        if(wavFarReader_){
            wavfile_reader_close(wavFarReader_);
            wavFarReader_ = NULL;
        }
        frameSizeInSamples_ = 0;
        reallocInternalBuf(0);
    }
    
    virtual void setFrameDuration(int frameMilliSeconds) override{
        frameSizeInSamples_ = frameMilliSeconds*farInfo_.samplerate/1000;
        reallocInternalBuf(frameSizeInSamples_);
    }
    
    virtual int getSamplerate()override{
        return farInfo_.samplerate;
    }
    virtual int getChannels()override{
        return 1;
    }
    
    void setFrame(XAudioFrame * next, wavfileinfo &info, int numSamples, XAudioType typ){
        next->type = typ;
        next->timestamp = 0; // TODO:
        next->samplesPerChannel = numSamples;
        next->numChannels = 1;
        next->samplerate = info.samplerate;
    }
    
    virtual void readNext(XAudioFrame * &farFrame, XAudioFrame * &nearFrame) override{
        farFrame = NULL;
        nearFrame = NULL;
        int numRead = wavfile_reader_read_short(wavFarReader_, sampleBuf_, numSample2Read_);
        if(numRead != numSample2Read_){
            return;
        }
        farFrame = frameFactory_->attainFrame(frameSizeInSamples_);
        nearFrame = frameFactory_->attainFrame(frameSizeInSamples_);
        for(int i = 0; i < frameSizeInSamples_; i++){
            nearFrame->samples[i] = sampleBuf_[i*farInfo_.channels + 0];
            farFrame->samples[i] = sampleBuf_[i*farInfo_.channels + 1];
        }
        this->setFrame(farFrame, farInfo_, frameSizeInSamples_, XAudioType::PLAYING);
        this->setFrame(nearFrame, farInfo_, frameSizeInSamples_, XAudioType::MIC);
    }
    
};



class WavFileAEC{
protected:
    std::string name_;
public:
    WavFileAEC(){
        makeObjName("WAVE-AEC", name_);
    }
    int run(const char * farWaveFilename, const char * nearWaveFilename, const char * outWaveFilename, XAudioEchoCanceller * aec, int delay){
        XAudioFrameFactory * frameFactory = new XAudioFrameFactory();
        AECSource * source = new WaveAECSource(frameFactory);
        wavfile_writer_t wavwriter = NULL;
//        int delay = 120; // 120, 134, 260
        int ret = -1;
        do{
            int frameDuration = aec->preferFrameDuration(); // 10;
            odbgi("system delay %d, frame-duration %d", delay, frameDuration);
            
            if(farWaveFilename && nearWaveFilename){
                WaveAECSource * wavsrc = new WaveAECSource(frameFactory);
                source = wavsrc;
                ret = wavsrc->open(farWaveFilename, nearWaveFilename, delay);

            }else{
                SingleWaveAECSource * wavsrc = new SingleWaveAECSource(frameFactory);
                source = wavsrc;
                ret = wavsrc->open(farWaveFilename, delay);
            }
            if(ret){
                break;
            }
            source->setFrameDuration(frameDuration);
            
            wavwriter = wavfile_writer_open(outWaveFilename, 1, source->getSamplerate());
            if(!wavwriter){
                ret = -1;
                odbge("fail to open wave [%s]", outWaveFilename);
                break;
            }
            odbgi("opened wave [%s]", outWaveFilename);
            
            
            int aecDelay = 0;
            int numChannels = source->getChannels();
            int frameSamplsPerChannel = frameDuration * source->getSamplerate() / 1000;
            ret = aec->open(aecDelay, source->getSamplerate(), numChannels, frameSamplsPerChannel );
            if(ret){
                odbge("fail to open aec [%s]", aec->name().c_str());
                break;
            }
            odbgi("opened aec [%s], delay %d", aec->name().c_str(), aecDelay);
            
            
            int numFrames = 0;
            short outbuffer[source->getSamplerate()];
            XAudioFrame * farFrame = NULL;
            XAudioFrame * nearFrame = NULL;
            source->readNext(farFrame, nearFrame);
            while(farFrame && nearFrame){
                ret = 0;
                
                if(farFrame){
                    int numSamples = farFrame->samplesPerChannel * numChannels;
                    ret = aec->processFar(farFrame->samples, numSamples);
                    if(ret){
                        odbge("fail to aec processFar, error=%d", ret);
                        break;
                    }
//                    ret = wavfile_writer_write_short(wavwriter, farFrame->samples, farFrame->samplesPerChannel);
                    frameFactory->returnFrame(farFrame);
                }

                if(nearFrame){
                    int numSamples = nearFrame->samplesPerChannel * numChannels;
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
//                    ret = wavfile_writer_write_short(wavwriter, nearFrame->samples, nearFrame->samplesPerChannel);
                    frameFactory->returnFrame(nearFrame);
                }
                
                source->readNext(farFrame, nearFrame);
            }
            if(farFrame){
                frameFactory->returnFrame(farFrame);
                farFrame = NULL;
            }
            if(nearFrame){
                frameFactory->returnFrame(nearFrame);
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
        
        if(source){
            delete source;
            source = NULL;
        }
        
        if(frameFactory){
            delete frameFactory;
            frameFactory = NULL;
        }
        return ret;
    }
    
    static int exec(const char * farWaveFilename, const char * nearWaveFilename, const char * outWaveFilename, XAudioEchoCanceller * aec, int delay);
};
int WavFileAEC::exec(const char * farWaveFilename, const char * nearWaveFilename, const char * outWaveFilename, XAudioEchoCanceller * aec, int delay){
    WavFileAEC obj;
    return obj.run(farWaveFilename, nearWaveFilename, outWaveFilename, aec, delay);
}

#include "webrtc/modules/audio_processing/aec/aec_core.h"
extern "C"{
    void aec_fft(float time_data[PART_LEN2], float freq_data[2][PART_LEN1]) ;
    void aec_fft2(float time_data[PART_LEN2], float freq_data[2][PART_LEN1]) ;
    void aec_scaled_inverse_fft(float freq_data[2][PART_LEN1],
                                float time_data[PART_LEN2],
                                float scale,
                                int conjugate);
    int aec_get_sqrt_hanning(float * data);
}

static int BitCount(uint32_t u32) {
    uint32_t tmp = u32 - ((u32 >> 1) & 033333333333) -
    ((u32 >> 2) & 011111111111);
    tmp = ((tmp + (tmp >> 3)) & 030707070707);
    tmp = (tmp + (tmp >> 6));
    tmp = (tmp + (tmp >> 12) + (tmp >> 24)) & 077;
    
    return ((int) tmp);
}

static inline
int xxcorr_convert_index(int s){
    return (s) < PART_LEN ? (s) + PART_LEN : (s)-PART_LEN;
}

static
int xxcorr(float x1[PART_LEN], float x2[PART_LEN], float Cxy[PART_LEN2]){
    float ex1[PART_LEN2];
    float ex2[PART_LEN2];
    float Y1[2][PART_LEN1];
    float Y2[2][PART_LEN1];
    float S12[2][PART_LEN1];
    
    memcpy(ex1, x1, PART_LEN*sizeof(x1[0]));
    memcpy(ex2, x2, PART_LEN*sizeof(x2[0]));
    memset(&ex1[PART_LEN], 0, PART_LEN * sizeof(ex1[0]));
    memset(&ex2[PART_LEN], 0, PART_LEN * sizeof(ex2[0]));
    
    aec_fft(ex1, Y1);
    aec_fft(ex2, Y2);
    
    // S12=Y1.*conj(Y2);
    for(int i = 0; i < PART_LEN1; i++){
        S12[0][i] = Y1[0][i]*Y2[0][i] + Y1[1][i]*Y2[1][i];
        S12[1][i] = Y1[1][i]*Y2[0][i] - Y1[0][i]*Y2[1][i];
    }
    
    //time12=ifft(S12);
    aec_scaled_inverse_fft(S12, Cxy, 2.0f, 0);
    
//    for(int i = 0; i < PART_LEN; i++){
//        float *pa = &Cxy[i];
//        float *pb = &Cxy[PART_LEN+i];
//        float temp = *pa;
//        *pa = *pb;
//        *pb = temp;
//    }
    
    return 0;
}

void aec_dig_out(){
    const std::string name_="Test-FFT";
    
    odbgi("");
    odbgi("-------------- aec_dig_out --------------");
    int verbose = 0;
    // test FFT
    {
        odbgi("");
        float time_data1[PART_LEN2];
        float time_data2[PART_LEN2];
        float freq_data1[2][PART_LEN1];
        float freq_data2[2][PART_LEN1];
        memset(time_data1, 0, sizeof(time_data1));
        for(int i = 0; i < PART_LEN2; ++i){
            time_data1[i] = i+1;
            time_data2[i] = i+1;
        }
        aec_fft(time_data1, freq_data1);
        aec_fft2(time_data2, freq_data2);
        bool equal = true;
        for(int i = 0; i < PART_LEN1; ++i){
            if(freq_data1[0][i] != freq_data2[0][i]
               || freq_data1[1][i] != freq_data2[1][i]){
                equal = false;
                break;
            }
        }
        odbgi("fft %s fft2", equal?"==": "!=");
        if(verbose){
            // print FFT data
            for(int i = 0; i < PART_LEN1; ++i){
                odbgi("freq_data[%d]: %+4.4f    %+4.4f", i, freq_data1[0][i], freq_data1[1][i]);
            }
            odbgi("-----");
            float* x_fft_ptr = NULL;
            x_fft_ptr = &freq_data1[0][0];
            for (int i = 0; i < PART_LEN1; i++) {
                odbgi("x_fft_ptr[%d]: %+4.4f    %+4.4f", i, x_fft_ptr[i], x_fft_ptr[PART_LEN1 + i]);
            }
        }
    }



    // test inverse FFT
    {
        odbgi("");
        float std_time_data[PART_LEN2];
        float time_data_for_fft[PART_LEN2];
        float freq_data_for_fft[2][PART_LEN1];
        float inv_time_data[PART_LEN2];
        for(int i = 0; i < PART_LEN2; ++i){
            std_time_data[i] = i+1;
        }
        memcpy(time_data_for_fft, std_time_data, sizeof(std_time_data));
        aec_fft(time_data_for_fft, freq_data_for_fft);
        aec_scaled_inverse_fft(freq_data_for_fft, inv_time_data, 2.0f, 0);
        float max_diff = 0;
        for(int i = 0; i < PART_LEN2; ++i){
            float diff = std::abs(inv_time_data[i] - std_time_data[i]);
            if(diff > max_diff){
                max_diff = diff;
            }
        }
        odbgi("inverse_fft max_diff=%.8f", max_diff);
    }

    
    // test Hanning window
    {
        odbgi("");
        int winSize = aec_get_sqrt_hanning(NULL);
        float winData[winSize];
        aec_get_sqrt_hanning(winData);
        odbgi("hanning winSize = %d", winSize);
        if(verbose){
            for(int i = 0; i < winSize/2+1; ++i){
                int idx1 = i;
                int idx2 = winSize-1-i;
                float val = winData[idx1] + winData[idx2];
                odbgi("sum(%d,%d)=sum(%.4f,%.4f)=%.4f", idx1, idx2, winData[idx1], winData[idx2], val);
            }
        }
    }
    
    // test xcorr
    {
        odbgi("");
        
        // matlab
//        d = 20;
//        N = 64;
//        x1 = [1:N];
//        x2 = [d:N,1:d-1];
//        ex1=[x1, zeros(1,N)];
//        ex2=[x2, zeros(1,N)];
//        Y1=fft(ex1);
//        Y2=fft(ex2);
//        S12=Y1.*conj(Y2);
//        time12=ifft(S12);
//        time12_real=real(time12);
//        Cxy=fftshift(time12_real);
//        [max,location]=max(Cxy);
//        d2=location-N ;
        
        int d = -21; // delay
        float Fs=500; // samplerate
        int N = PART_LEN;
        float x1[PART_LEN];
        float x2[PART_LEN];
        float ex1[2*N];
        float ex2[2*N];
        float Y1[2][PART_LEN1];
        float Y2[2][PART_LEN1];
        float S12[2][PART_LEN1];
        float Cxy[PART_LEN2];
        
        // std Cxy of delay = -21 from matlab
        static float stdCxy[PART_LEN2]={
            0.0000, 0.5358, 0.9574, 1.2504, 1.4046, 1.4134, 1.2749, 0.9912,
            0.5688, 0.0182, -0.6461, -1.4061, -2.2404, -3.1252, -4.0344, -4.9403,
            -5.8145, -6.6281, -7.3529, -7.9618, -8.4294, -8.7329, -8.8525, -8.7721,
            -8.4797, -7.9678, -7.2337, -6.2796, -5.1131, -3.7468, -2.1983, -0.4900,
            1.3510, 3.2939, 5.3041, 7.3440, 9.3739, 11.3525, 13.2379, 14.9882,
            16.5625, 17.9218, 19.0296, 19.8529, 19.3083, 18.5843, 17.6922, 16.6443,
            15.4531, 14.1315, 12.6930, 11.1508, 9.5186, 7.8099, 6.0384, 4.2179,
            2.3619, 0.4845, -1.4006, -3.2793, -5.1375, -6.9607, -8.7346, -10.4444,
            -12.0753, -14.0478, -15.7525, -17.1777, -18.3162, -19.1659, -19.7291, -20.0123,
            -20.0260, -19.7842, -19.3042, -18.6055, -17.7099, -16.6409, -15.4228, -14.0807,
            -12.6397, -11.1244, -9.5587, -7.9655, -6.3658, -4.7791, -3.2228, -1.7124,
            -0.2607, 1.1215, 2.4255, 3.6452, 4.7762, 5.8162, 6.7645, 7.6218,
            8.3901, 9.0721, 9.6715, 10.1921, 10.6377, 11.0121, 11.3186, 11.5601,
            11.7384, 11.8548, 11.9093, 11.9010, 11.6305, 11.1923, 10.6087, 9.9040,
            9.1035, 8.2331, 7.3186, 6.3853, 5.4571, 4.5562, 3.7027, 2.9139,
            2.2042, 1.5849, 1.0637, 0.6449, 0.3293, 0.1144, -0.0057, -0.0400,
        };
        
        for(int i = 0; i < N; ++i){
            x1[i] = cos(2*M_PI*10*i/Fs);
        }
        int esize = sizeof(x1[0]);
        int offset = abs(d);
        if(d > 0){
            memcpy(&x2[0],        &x1[offset],   sizeof(esize)*(N-offset));
            memcpy(&x2[N-offset], &x1[0],        sizeof(esize)*(offset));
        }else{
            memcpy(&x2[0],        &x1[N-offset], sizeof(esize)*(offset));
            memcpy(&x2[offset],   &x1[0],        sizeof(esize)*(N-offset));
        }
        
        
        xxcorr(x1, x2, Cxy);
        int location = 0;
        float max = Cxy[0];
        odbgi("");
        for(int i = 0; i < (N*2); i++){
            if(max < Cxy[i]){
                max = Cxy[i];
                location = i;
            }
            odbgi("Cxy[%3d]=%.6f", xxcorr_convert_index(i), Cxy[i]);
        }
        location = xxcorr_convert_index(location);
        
        int d2=location-N ;
        odbgi("location=%d, d2=%d, max=%.6f", location, d2, max);
        
        int CxyIdx = 0;
        int stdIdx = 0;
        float maxCxyDiff = 0;
        for(int i = 0; i < 2*N; ++i){
            int index = xxcorr_convert_index(i);
            float diff = fabs(Cxy[i] - stdCxy[index]);
            if(maxCxyDiff < diff){
                maxCxyDiff = diff;
                CxyIdx = i;
                stdIdx = index;
            }
        }
        odbgi("maxCxyDiff=%.6f, Cxy[%d]=%.6f, stdCxy[%d]=%.6f", maxCxyDiff, CxyIdx, Cxy[CxyIdx], stdIdx, stdCxy[stdIdx]);
    }
    
}

int main(int argc, char** argv){
    
    const char * outWaveFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/tmp/macout.wav";
    int ret = 0;
    
    const char * tlvFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/testdata/xiaomi5/001-src-aec.tlv";
    const char * farWaveFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/tmp/xiaomi5-001-far.wav";
    const char * nearWaveFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/tmp/xiaomi5-001-near.wav";
    
    bool do_TLV_AEC = false;
    if(do_TLV_AEC){
        ret = TLVFileAEC::exec(tlvFilename, outWaveFilename, new XAudioWebrtcAEC(), 200);
        //    ret = TLVFileAEC::exec(tlvFilename, outWaveFilename, new XAudioWebrtcAECM(), 120);
    }

    bool do_TLV2Wave = false;
    if(do_TLV2Wave){
        ret = TLV2Wave::exec(tlvFilename, farWaveFilename, XAudioType::PLAYING);
        ret = TLV2Wave::exec(tlvFilename, nearWaveFilename, XAudioType::MIC);
    }
    

    ret = WavFileAEC::exec(farWaveFilename, nearWaveFilename, outWaveFilename, new XAudioWebrtcAEC(), 119);
//    ret = WavFileAEC::exec(farWaveFilename, nearWaveFilename, outWaveFilename, new XAudioSpeexAEC(true), 119);
////    ret = WavFileAEC::exec(farWaveFilename, nearWaveFilename, outWaveFilename, new XAudioWebrtcAECM(), 120);
////    ret = WavFileAEC::exec(farWaveFilename, nearWaveFilename, outWaveFilename, new XAudioSabineSpeexAEC(), 100);

//    const char * farWaveFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/testdata/hua_ref.wav";
//    const char * nearWaveFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/testdata/hua_mic.wav";
//    ret = WavFileAEC::exec(farWaveFilename, nearWaveFilename, outWaveFilename, new XAudioSpeexAEC(true), 0);
    
//    const char * stereoFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/testdata/std_stereo.wav";
//    ret = WavFileAEC::exec(stereoFilename, NULL, outWaveFilename, new XAudioSpeexAEC(false), 0);
    
    aec_dig_out();
    
    return ret;
}
