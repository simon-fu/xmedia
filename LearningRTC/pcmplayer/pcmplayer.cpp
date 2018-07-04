
#include <string>
#include <stdio.h>
#include <SDL2/SDL.h>


#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)

struct AudioBuffer{
	SDL_AudioDeviceID devId = 0;
	uint8_t * data = NULL;
	int offset = 0;
	int length = 0;
	bool playDone = false;
};


// 音频播发设备需要更多的数据用来播放
// 
// udata - 打开音频设备时SDL_AudioSpec里的userdata
// stream - 把数据填到这个地址
// len - 需要的数据长度，一定要填充到这个长度，如果没有足够的数据要补0（相当于静音）
static
void  audio_need_more_callback(void *udata, Uint8 *stream, int len){ 
	static const std::string name_ = "callback";
	AudioBuffer * audioBuffer = (AudioBuffer *) udata;
	odbgi("data=%d/%d, reqlen=%d", audioBuffer->offset, audioBuffer->length, len);
	if(audioBuffer->offset >= audioBuffer->length){
		SDL_memset(stream, 0, len);// 静音
		if(!audioBuffer->playDone){
			odbgi("playing done");
			// 音频数据已经播放完毕，停止播放
			audioBuffer->playDone = true;
			SDL_PauseAudioDevice(audioBuffer->devId, 1);
	 		// 发送退出事件给主线程
			SDL_Event event;
			event.type = SDL_QUIT;
			SDL_PushEvent(&event);
		}
		return;	
	}
	
	// 计算还剩多少音频数据
	int remains = audioBuffer->length - audioBuffer->offset;

	// 取len和remains最小值
	int bytes = len <= remains ? len : remains;

	// 拷贝音频数据到SDL的stream里，并更新偏移
	memcpy(stream, audioBuffer->data+audioBuffer->offset, bytes);
	audioBuffer->offset += bytes;

	// 如果填不满stream，则补0
	if(bytes < len ){
		memset(stream+bytes, 0, len-bytes);
	}
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

        // 设置要音频设备参数
		SDL_AudioSpec wanted_spec;
		wanted_spec.freq = samplerate; 
		wanted_spec.format = AUDIO_S16SYS; 
		wanted_spec.channels = channels; 
		wanted_spec.silence = 0; 
		wanted_spec.samples = 1024; 
		wanted_spec.callback = audio_need_more_callback; // 回调函数
		wanted_spec.userdata = &audioBuffer; 

		// 打开音频播放设备
		audioBuffer.devId = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, NULL, 0);
		if(audioBuffer.devId == 0){
			odbge("can't open SDL audio"); 
			ret = -11;
			break; 
		}
		odbgi("playing audio device %d", audioBuffer.devId);

		// 开始播放音频
		// SDL不断调用回调函数，拉取音频数据
		SDL_PauseAudioDevice(audioBuffer.devId, 0);

		// 等待播放完毕
		SDL_Event event;
        while(1){
        	SDL_WaitEvent(&event);
			if(event.type==SDL_QUIT){
				odbgi("got QUIT event %d", event.type);
				break;
			}
        }// while

		ret = 0;
	}while(0);

	if(fp){
		fclose(fp);
		fp = NULL;
	}
	if(audioBuffer.devId > 0){
		// 关闭音频播放设备
		SDL_CloseAudioDevice(audioBuffer.devId);
		audioBuffer.devId = 0;
	}
	if(audioBuffer.data){
		delete[] audioBuffer.data;
		audioBuffer.data = NULL;
	}
	return ret;
}

static
std::string getFilePath(const std::string& fname){
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
    return path + fname;
}
int main(int argc, char* argv[]){
	std::string name_ = "main";
	int ret = SDL_Init(SDL_INIT_AUDIO);
	if(ret){
		odbge( "Could not initialize SDL - %s\n", SDL_GetError()); 
		return -1;
	}

    std::string filepath = getFilePath("../data/test_s16le_44100Hz_2ch.pcm");
	playPCMFile(filepath.c_str(), 44100, 2);

	SDL_Quit();

	return 0;
}


