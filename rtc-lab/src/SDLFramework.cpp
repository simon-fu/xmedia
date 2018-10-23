

#include "SDLFramework.hpp"

#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "SDLFrW", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "SDLFrW", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "SDLFrW", ##ARGS); printf("\n"); fflush(stdout); }while(0)

struct AudioBuffer{
    uint8_t * data = NULL;
    int offset = 0;
    int length = 0;
};

static
int64_t get_now_ms(){
    unsigned long milliseconds_since_epoch =
    std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count();
    return milliseconds_since_epoch;
};

static
int feedAudioData(AudioBuffer * audioBuffer, SDLAudioPlayer& player){
    int bytes2read = player.getFrameSize() * player.getChannels() * sizeof(int16_t);
    int64_t start_ms = get_now_ms();
    int64_t playSamples = 0;
    int numSamples = 0;
    do{
        // 计算还剩多少音频数据
        int remains = audioBuffer->length - audioBuffer->offset;
        
        // 取bytes2read和remains最小值
        int bytes = bytes2read <= remains ? bytes2read : remains;
        numSamples = bytes/sizeof(int16_t);
        
        player.queueSamples((int16_t*)(audioBuffer->data+audioBuffer->offset), numSamples);
        audioBuffer->offset += bytes;
        
        playSamples += numSamples;
        int64_t duration = player.getTimeMsOfSamples(playSamples/player.getChannels());
        int64_t elapsed_ms = get_now_ms()-start_ms;
        if(duration > elapsed_ms){
            int ms = (int)(duration-elapsed_ms);
            odbgi("sleep %d ms", ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
    }while(numSamples > 0);
    
    odbgi("waiting for playing complete");
    player.waitForComplete();
    odbgi("play complete");
    
    return 0;
}

static
int playPCMFile(const char * filename, int samplerate, int channels){
    std::string name_ = "player";
    int ret = -1;
    FILE * fp = NULL;
    AudioBuffer audioBuffer;
    
    do{
        // 打开文件
        fp = fopen(filename, "rb");
        if (!fp) {
            odbge("fail to open file [%s]", filename);
            ret = -1;
            break;
        }
        
        // 移动读取位置到文件末尾；
        // 获取文件长度；
        // 再恢复读取位置到文件开头；
        fseek(fp, 0, SEEK_END);
        long fileSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        odbgi("pcm file: [%s]", filename);
        odbgi("file size: [%ld]", fileSize);
        odbgi("samplerate: [%d]", samplerate);
        odbgi("channels: [%d]", channels);
        
        // 分配音频数据内存;
        // 一次把所有的音频数据读入到内存;
        audioBuffer.data = new uint8_t[fileSize];
        size_t bytes = fread(audioBuffer.data, sizeof(char), fileSize, fp);
        odbgi("read bytes %lld", (long long)bytes);
        if(bytes <= 0){
            odbge("can't read pcm file");
            ret = -5;
            break;
        }
        audioBuffer.length = (int) bytes;
        
        int samplesPerCh = 1024;
        SDLAudioPlayer player;
        ret = player.open(samplerate, channels, samplesPerCh);
        if(ret){
            odbge("can't open SDL audio, ret=%d", ret);
            break;
        }
        player.play(true);
        
        feedAudioData(&audioBuffer, player);
        
        ret = 0;
    }while(0);
    
    if(fp){
        fclose(fp);
        fp = NULL;
    }
    
    if(audioBuffer.data){
        delete[] audioBuffer.data;
        audioBuffer.data = NULL;
    }
    return ret;
}

