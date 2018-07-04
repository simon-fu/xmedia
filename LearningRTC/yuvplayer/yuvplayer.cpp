
#include <string>
#include <stdio.h>
#include <SDL2/SDL.h>


#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, name_.c_str(), ##ARGS); printf("\n"); fflush(stdout); }while(0)

class YUVRenderer{
protected:
	int width_;
	int height_;
	std::string name_;
	SDL_Window * window_ = NULL; 
	SDL_Renderer* sdlRenderer_ = NULL;
	SDL_Texture* sdlTexture_ = NULL;
	SDL_Rect sdlRect_;

	void freeMe(){
		if(sdlTexture_){
			SDL_DestroyTexture(sdlTexture_);
			sdlTexture_ = NULL;
		}
		if(sdlRenderer_){
			SDL_DestroyRenderer(sdlRenderer_);
			sdlRenderer_ = NULL;
		}
		if(window_){
			SDL_DestroyWindow(window_);
			window_ = NULL;
		}
	}

	int initMe(){
		int ret = -1;
		do{
			window_ = SDL_CreateWindow(name_.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width_, height_,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
			if(!window_) {  
				odbge("SDL: could not create window - exiting:%s\n",SDL_GetError());  
				ret = -11;
				break;
			}
			sdlRenderer_ = SDL_CreateRenderer(window_, -1, 0);  
			//IYUV: Y + U + V  (3 planes)
			//YV12: Y + V + U  (3 planes)
			sdlTexture_ = SDL_CreateTexture(sdlRenderer_, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width_, height_);  
			ret = 0;
		}while(0);
		if(ret){
			freeMe();
		}
		return ret;
	}

public:
	YUVRenderer(const std::string& name, int w, int h):name_(name), width_(w), height_(h){
		sdlRect_.x = 0;
		sdlRect_.y = 0;
		sdlRect_.w = width_;
		sdlRect_.h = height_;
	}
	virtual ~YUVRenderer(){
		freeMe();
	}

	void draw(const void * pixels, int linesize){
		SDL_UpdateTexture( sdlTexture_, NULL, pixels, linesize);  
		this->refresh();
	}

	void refresh(){
		// refresh last image
		SDL_RenderClear( sdlRenderer_ );  
		//SDL_RenderCopy( sdlRenderer_, sdlTexture_, &sdlRect_, &sdlRect_ );  
		SDL_RenderCopy( sdlRenderer_, sdlTexture_, NULL, NULL);  
		SDL_RenderPresent( sdlRenderer_ ); 
	}

	static YUVRenderer * create(const std::string& name, int w, int h, int * error);
};

YUVRenderer * YUVRenderer::create(const std::string& name, int w, int h, int * error){
	YUVRenderer * renderer = new YUVRenderer(name, w, h);
	int ret = renderer->initMe();
	if(ret){
		delete renderer;
		renderer  = NULL;
	}
	if(error){
		*error = ret;
	}
	return renderer;
}


static int playYUVFile(const char * filename, int width, int height, int framerate, bool loop = false){
	std::string name_ = "player";
	int ret = -1;
	FILE * fp = NULL;
	YUVRenderer * renderer = NULL;
	int bitsPerPixel = 12;
	int imageSize = width*height*bitsPerPixel/8;
	uint8_t * buffer = new uint8_t[imageSize];
	int frameInterval = 1000/framerate;
	do{
        fp = fopen(filename, "rb");
        if (!fp) {
        	odbge("fail to open yuv file [%s]", filename);
            ret = -1;
            break;
        }
        odbgi("yuv file: [%s]", filename);
        odbgi("video size: [%dx%d]", width, height);
        odbgi("framerate: [%d] (%d ms)", framerate, frameInterval);

        // TODO: check result of creating renderer 
		renderer = YUVRenderer::create("renderer1", width, height, &ret);

		SDL_Event event;
		bool quitLoop = false;
		bool playDone = false;
        int nframes = 0;
        uint32_t nextDrawTime = SDL_GetTicks();
        while(!quitLoop){
        	uint32_t now_ms = SDL_GetTicks();
        	if(now_ms >= nextDrawTime){
		        if(!playDone){
	        		// read and draw one frame
			        size_t bytes = fread(buffer, sizeof(char), imageSize, fp);
			        if(bytes != imageSize){
			        	if(nframes > 0){
			        		playDone = !loop;
			        		fseek(fp, 0, SEEK_SET);
			        		nframes = 0;
			        		odbgi("reach file end, loop=%d", loop);
			        		continue;
			        	}else{
				        	odbge("fail to read yuv file");
				            ret = -1;
				            break;
			        	}
			        }
			        renderer->draw(buffer, width);
			        ++nframes;
		        }
		        nextDrawTime += frameInterval;//SDL_Delay(40);
        	}

        	now_ms = SDL_GetTicks();
        	if(now_ms < nextDrawTime){
        		int timeout = (int)(nextDrawTime - now_ms);
        		SDL_WaitEventTimeout(NULL, timeout);
        	}
        	
			while (SDL_PollEvent(&event)) {
				// odbgi("got event %d", event.type);
				if(event.type==SDL_QUIT){
					odbgi("got QUIT event %d", event.type);
					quitLoop = true;
					break;
				}else if(event.type==SDL_WINDOWEVENT){
					if(event.window.event == SDL_WINDOWEVENT_CLOSE){
						odbgi("Window %d closed", event.window.windowID);
						quitLoop = true;
						break;
					}else if(event.window.event == SDL_WINDOWEVENT_RESIZED){
			            odbgi("Window %d resized to %dx%d",
			                    event.window.windowID, event.window.data1,
			                    event.window.data2);
			            renderer->refresh();
					}
				}
			}
        }// while(!quitLoop)

		ret = 0;
	}while(0);

	if(renderer){
		delete renderer;
		renderer = NULL;
	}
	if(fp){
		fclose(fp);
		fp = NULL;
	}
	if(buffer){
		delete[] buffer;
		buffer = NULL;
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
	int ret = SDL_Init(SDL_INIT_VIDEO) ;
	if(ret){
		odbge( "Could not initialize SDL - %s\n", SDL_GetError()); 
		return -1;
	}
    std::string filepath = getFilePath("../data/test_yuv420p_640x360.yuv");
	playYUVFile(filepath.c_str(), 640, 360, 25, true);

	SDL_Quit();

	return 0;
}

