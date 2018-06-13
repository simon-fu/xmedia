

#include <stdio.h>
#include <string>
#include <opus/opus.h>
#include "xwavfile.h"
#include "pesqmain.h"

#define odbgv(FMT, ARGS...) //do{  printf("|%10s|V| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgd(FMT, ARGS...) do{  printf("|%11s|D| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%11s|I| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%11s|E| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)

//static int webrtcDefaultSampleRate = 48000;
static int webrtcDefaultSampleRate = 16000;

int WebRtcOpus_PacketHasFec(const uint8_t* payload, size_t payload_length_bytes) {
    int frames, channels, payload_length_ms;
    int n;
    opus_int16 frame_sizes[48];
    const unsigned char *frame_data[48];
    
    if (payload == NULL || payload_length_bytes == 0)
        return 0;
    
    /* In CELT_ONLY mode, packets should not have FEC. */
    if (payload[0] & 0x80)
        return 0;
    
    payload_length_ms = opus_packet_get_samples_per_frame(payload, webrtcDefaultSampleRate) / 48;
    if (10 > payload_length_ms)
        payload_length_ms = 10;
    
    channels = opus_packet_get_nb_channels(payload);
    
    switch (payload_length_ms) {
        case 10:
        case 20: {
            frames = 1;
            break;
        }
        case 40: {
            frames = 2;
            break;
        }
        case 60: {
            frames = 3;
            break;
        }
        default: {
            return 0; // It is actually even an invalid packet.
        }
    }
    
    /* The following is to parse the LBRR flags. */
    if (opus_packet_parse(payload, (opus_int32)payload_length_bytes, NULL,
                          frame_data, frame_sizes, NULL) < 0) {
        return 0;
    }
    
    if (frame_sizes[0] <= 1) {
        return 0;
    }
    
    for (n = 0; n < channels; n++) {
        if (frame_data[0][0] & (0x80 >> ((n + 1) * (frames + 1) - 1)))
            return 1;
    }
    
    return 0;
}

/* For decoder to determine if it is to output speech or comfort noise. */
static int16_t DetermineAudioType(int& in_dtx_mode, size_t encoded_bytes) {
    // Audio type becomes comfort noise if |encoded_byte| is 1 and keeps
    // to be so if the following |encoded_byte| are 0 or 1.
    if (encoded_bytes == 0 && in_dtx_mode) {
        return 2;  // Comfort noise.
    } else if (encoded_bytes == 1) {
        in_dtx_mode = 1;
        return 2;  // Comfort noise.
    } else {
        in_dtx_mode = 0;
        return 0;  // Speech.
    }
}

static int DecodeNative(OpusDecoder * decoder, const uint8_t* encoded,
                        size_t encoded_bytes, int frame_size,
                        int16_t* decoded, int16_t* audio_type, int decode_fec) {
    int res = opus_decode(decoder, encoded, (opus_int32)encoded_bytes,
                          (opus_int16*)decoded, frame_size, decode_fec);
    
    if (res <= 0)
        return -1;
    
    int in_dtx_mode = 0;
    *audio_type = DetermineAudioType(in_dtx_mode, encoded_bytes);
    
    return res;
}


int WebRtcOpus_DecodeFec(OpusDecoder * decoder, const uint8_t* encoded,
                         size_t encoded_bytes, int16_t* decoded,
                         int16_t* audio_type) {
    int decoded_samples;
    int fec_samples;
    
    if (WebRtcOpus_PacketHasFec(encoded, encoded_bytes) != 1) {
        return 0;
    }
    
    fec_samples = opus_packet_get_samples_per_frame(encoded, webrtcDefaultSampleRate);
    
    decoded_samples = DecodeNative(decoder, encoded, encoded_bytes,
                                   fec_samples, decoded, audio_type, 1);
    if (decoded_samples < 0) {
        return -1;
    }
    
    return decoded_samples;
}

class DropStrategy{
    std::string name_;
public:
    DropStrategy(const std::string& name):name_(name){
    }
    virtual ~DropStrategy(){}
    virtual const std::string& name(){return name_;}
    virtual int dropRate() = 0;
    virtual bool nextDrop(int numFrames) = 0;
};
class DropRegular : public DropStrategy{
    int range_ = 5;
public:
    DropRegular(int rate):DropStrategy("RegularDrop"), range_(100/rate){
    }
    virtual ~DropRegular(){}
    virtual int dropRate() override{
        return 100*1/range_;
    }
    virtual bool nextDrop(int numFrames) override{
        bool drop = numFrames > 1 && (numFrames % range_ == 1);
        return drop;
    }
};
class DropRandom : public DropStrategy{
    int rate_ = 20;
public:
    DropRandom(int rate):DropStrategy("RandomDrop"), rate_(rate){
    }
    virtual ~DropRandom(){}
    virtual int dropRate() override{
        return rate_;
    }
    virtual bool nextDrop(int numFrames) override{
        bool drop = numFrames > 1 && (rate_>0 && rand()%100 < rate_);
        return drop;
    }
};


class OpusDropTest{
    std::string name_ = "OPUS-DROP";
    OpusEncoder * opusEnc_ = NULL;
    OpusDecoder * opusDec_ = NULL;
    wavfile_reader_t inputWavReader_ = NULL;
    wavfile_writer_t outputWavWriter_ = NULL;
    int sampleRate_ = 16000;
    int channels_ = 1;
public:
    void run(){
        //std::string inputWaveFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/testdata/hua_ref.wav";
        std::string inputWaveFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/testdata/woman.wav";
        std::string outputWaveFilename = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/aec/tmp/out-opus.wav";
        int enableFEC = 1;
        int packetLossPerc = 28;
        //DropRegular dropper(packetLossPerc);
        DropRandom dropper(packetLossPerc);
        
        
        wavfileinfo wavInfo;
        int64_t duration = 20;
        short * sampleBuf = new short[48000*2];
        int sampleBufSize = 48000*2;
        uint8_t encBuf[2048];
        int encBufSize = 2048;
        int ret = -1;
        do{

            srand((unsigned)time(NULL));
            inputWavReader_ = wavfile_reader_open(inputWaveFilename.c_str());
            if (!inputWavReader_) {
                ret = -1;
                odbge("fail to open [%s], ret=%d", inputWaveFilename.c_str(), ret);
                break;
            }
            wavfile_reader_info(inputWavReader_, &wavInfo);
            sampleRate_ = wavInfo.samplerate;
            channels_ = wavInfo.channels;
            odbgi("opened input wave [%s]", inputWaveFilename.c_str());
            if(sampleRate_ != 8000 && sampleRate_ != 16000){
                ret = -1;
                odbge("only support 8K or 16K, but sampleRate=%d", sampleRate_);
                break;
            }
            if(channels_ != 1){
                ret = -1;
                odbge("only support mono,but channels=%d", channels_);
                break;
            }
            
            outputWavWriter_ = wavfile_writer_open(outputWaveFilename.c_str(), channels_, sampleRate_);
            if (!outputWavWriter_) {
                ret = -1;
                odbge("fail to open [%s], ret=%d", outputWaveFilename.c_str(), ret);
                break;
            }
            
            
            int error = 0;
            opusEnc_ = opus_encoder_create(sampleRate_,
                                            channels_,
                                            OPUS_APPLICATION_VOIP,
                                            &error
                                            );
            if(!opusEnc_){
                odbge("create opus encoder fail with error %d", error);
                break;
            }
            
            if(enableFEC){
                ret = opus_encoder_ctl(opusEnc_, OPUS_SET_INBAND_FEC(1));
                if(ret){
                    odbgi("enable inband FEC fail with %d", ret);
                    break;
                }
                odbgi("enabled inband FEC");
                
                int lossRate = dropper.dropRate();
                ret = opus_encoder_ctl(opusEnc_, OPUS_SET_PACKET_LOSS_PERC(lossRate));
                if(ret){
                    odbgi("set loss fail with %d", ret);
                    break;
                }
                odbgi("set loss rate to %d", lossRate);
            }
            
            
            opusDec_ = opus_decoder_create(sampleRate_, channels_, &error);
            if(!opusDec_){
                odbge("create opus decoder fail with error %d", error);
                break;
            }
            
            int frameSamplesPerChannels = (int) (sampleRate_ * duration /1000) ;
            int numSamples = (int) (channels_ * frameSamplesPerChannels) ;
            int numReadFrames = 0;
            int numWriteFrames = 0;
            int numFECFrames = 0;
            int dropPrev = 0;
            int drop = 0;
            
            while(1){
                
                int numRead = wavfile_reader_read_short(inputWavReader_, sampleBuf, numSamples);
                if(numRead != numSamples){
                    ret = 0;
                    break;
                }
                
                
                
                int encBytes = opus_encode(opusEnc_, sampleBuf, frameSamplesPerChannels, encBuf, encBufSize);
                if(encBytes < 0){
                    odbge("opus_encode fail with %d", encBytes);
                    ret = -1;
                    break;
                }
                
                
                int toc_c  = encBuf[0]&0x3;
                int hasFEC = WebRtcOpus_PacketHasFec(encBuf, encBytes);
                drop = dropper.nextDrop(numReadFrames);
                int bufSize = sampleBufSize;
                int decSamples = 0;
                if(dropPrev){
                    // attempt to decode prev frame with in-band FEC
                    // if current frame drop, decode PLC
                    uint8_t * encData = drop ? NULL : encBuf;
                    int encLength = drop ? 0 : encBytes;
                    bufSize = frameSamplesPerChannels;
                    ret = opus_decode(opusDec_, encData, encLength, sampleBuf+decSamples, bufSize, 1);
                    if(ret < 0){
                        odbge("opus_decode FEC fail with %d", ret);
                        break;
                    }
                    decSamples += ret;
                }
                if(!drop){
                    // regular decode current frame
                    bufSize = sampleBufSize;
                    ret = opus_decode(opusDec_, encBuf, encBytes, sampleBuf+decSamples, bufSize, 0);
                    if(ret < 0){
                        odbge("opus_decode regular fail with %d", ret);
                        break;
                    }
                    decSamples += ret;
                }

                
                int numWrite = 0;
                if(decSamples > 0){
                    numWrite = (decSamples/frameSamplesPerChannels/channels_);
                    numWriteFrames += numWrite;
                    wavfile_writer_write_short(outputWavWriter_, sampleBuf, decSamples);
                }
                if(hasFEC){
                    ++numFECFrames;
                }
                ++numReadFrames;
                dropPrev = drop;
                ret = 0;
                odbgi("n-read %3d, n-write %3d (%+d); fec %d, c %d; drop %d", numReadFrames, numWriteFrames, numWrite, hasFEC, toc_c, drop);
                
            }
            if(ret) break;
            
            if(dropPrev){
                // decode PLC if last frame drop
                int bufSize = frameSamplesPerChannels;
                int decSamples = opus_decode(opusDec_, NULL, 0, sampleBuf, bufSize, 0);
                if(decSamples > 0){
                    int numWrite = (decSamples/frameSamplesPerChannels/channels_);
                    numWriteFrames += numWrite;
                    wavfile_writer_write_short(outputWavWriter_, sampleBuf, decSamples);
                    odbgi("decode last frame with PLC");
                }
                
            }
            
            if(outputWavWriter_){
                wavfile_writer_close(outputWavWriter_);
                outputWavWriter_ = NULL;
            }
            
            float pesq_mos = 0.0;
            float mapped_mos = 0.0;
            ret = pesq_wav(sampleRate_, inputWaveFilename.c_str(), outputWaveFilename.c_str(), &pesq_mos, &mapped_mos);
            if(ret){
                break;
            }
            odbgi("--- final ---");
            odbgi("  read frames:    %d", numReadFrames);
            odbgi("  write frames:   %d", numWriteFrames);
            odbgi("  FEC frames:     %d (%d%%)", numFECFrames, 100*numFECFrames/numWriteFrames);
            odbgi("  drop:           [%s]-[%d%%]", dropper.name().c_str(), dropper.dropRate());
            odbgi("  MOS:            [%.3f]-[%.3f]", pesq_mos, mapped_mos);
        }while(0);
        
        if(opusEnc_){
            opus_encoder_destroy(opusEnc_);
            opusEnc_ = NULL;
        }
        
        if(opusDec_){
            opus_decoder_destroy(opusDec_);
            opusDec_ = NULL;
        }
        
        if(outputWavWriter_){
            wavfile_writer_close(outputWavWriter_);
            outputWavWriter_ = NULL;
        }
        
        if(inputWavReader_){
            wavfile_reader_close(inputWavReader_);
            inputWavReader_ = NULL;
        }
        
        if(sampleBuf){
            delete[] sampleBuf;
            sampleBuf = NULL;
        }

    }
};

int main(int argc, char** argv){
    OpusDropTest tester;
    tester.run();
    return 0;
}
