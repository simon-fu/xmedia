

#ifndef SDLFramework_hpp
#define SDLFramework_hpp

#include <stdio.h>
#include <string>
#include <list>
#include <chrono>
#include <thread>
//#include <map>
//#include <vector>

extern "C"{
#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
}

class SDLWindow{
public:
    virtual ~SDLWindow(){}
    virtual void refresh() = 0;
    virtual SDL_Renderer* getRenderer() = 0;
    virtual int getWidth() = 0;
    virtual int getHeight() = 0;
};

class SDLView{
protected:
    SDLWindow * win_ = NULL;
    int x_ = 0;
    int y_ = 0;
    SDL_Texture* sdlTexture_ = NULL;
    
public:
    SDLView(){
    }
    virtual ~SDLView(){
        if(sdlTexture_){
            SDL_DestroyTexture(sdlTexture_);
            sdlTexture_ = NULL;
        }
    }
    virtual void getPosition(int &x, int &y){
        x = x_;
        y = y_;
    }
    virtual void setPosition(int x, int y){
        x_ = x;
        y_ = y;
    }
    virtual void setWindow(SDLWindow * win){
        win_ = win;
    }
    virtual void onDraw() = 0;
    
};

class SDLWindowImpl : public SDLWindow{
protected:
    int width_;
    int height_;
    std::string name_;
    SDL_Window * window_ = NULL;
    SDL_Renderer* sdlRenderer_ = NULL;
    std::list<SDLView *> subViews;
public:
    SDLWindowImpl(const std::string& name, int w, int h):name_(name), width_(w), height_(h){
    }
    
    virtual ~SDLWindowImpl(){
        this->close();
    }
    
    int open(int x = -1, int y = -1){
        int ret = -1;
        do{
            if(x < 0)  x = SDL_WINDOWPOS_CENTERED;
            if(y < 0)  y = SDL_WINDOWPOS_CENTERED;
            window_ = SDL_CreateWindow(name_.c_str(), x, y, width_, height_,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
            if(!window_) {
                //odbge("SDL: could not create window - exiting:%s\n",SDL_GetError());
                ret = -11;
                break;
            }
            sdlRenderer_ = SDL_CreateRenderer(window_, -1, 0);
            SDL_RaiseWindow(window_);
            ret = 0;
        }while(0);
        if(ret){
            this->close();
        }
        return ret;
    }
    
    void close(){
        for(auto v : subViews){
            delete v;
        }
        subViews.clear();
        
        if(sdlRenderer_){
            SDL_DestroyRenderer(sdlRenderer_);
            sdlRenderer_ = NULL;
        }
        if(window_){
            SDL_DestroyWindow(window_);
            window_ = NULL;
        }
    }
    
    void setPosition(int x, int y){
        
    }
    
    void addView(SDLView * view){
        for(auto v : subViews){
            if(v == view ) return;
        }
        subViews.push_back(view);
        view->setWindow(this);
        this->refresh();
    }
    
    virtual void refresh() override{
        SDL_RenderClear( sdlRenderer_ );
        for(auto v : subViews){
            v->onDraw();
        }
        SDL_RenderPresent( sdlRenderer_ );
    }
    
    virtual SDL_Renderer* getRenderer() override{
        return sdlRenderer_;
    }
    
    virtual int getWidth()override{
        return width_;
    }
    virtual int getHeight()override{
        return height_;
    }
};

class SDLSurfaceView : public SDLView{
protected:
    SDL_Surface * surface_ = NULL;
    SDL_Rect rect_;
    
    void freeImage(){
        if(sdlTexture_){
            SDL_DestroyTexture(sdlTexture_);
            sdlTexture_ = NULL;
        }
        
        if(surface_){
            SDL_FreeSurface(surface_);
            surface_ = NULL;
        }
    }
public:
    SDLSurfaceView(){
    }
    
    virtual ~SDLSurfaceView(){
        this->freeImage();
    }
    
    void getImageSize(int &w, int &h){
        if(surface_){
            w = surface_->w;
            h = surface_->h;
        }else{
            w = 0;
            h = 0;
        }
    }
    
    int draw(SDL_Surface * surface){
        this->freeImage();
        surface_ = surface;
        
        if(win_){
            win_->refresh();
        }
        return 0;
    }
    
    virtual void onDraw()override{
        if(!sdlTexture_ && surface_ && win_){
            sdlTexture_ = SDL_CreateTextureFromSurface(win_->getRenderer(), surface_);
            rect_.x = x_;
            rect_.y = y_;
            SDL_QueryTexture(sdlTexture_, NULL, NULL, &rect_.w, &rect_.h);
            SDL_FreeSurface(surface_);
            surface_ = NULL;
        }
        if(sdlTexture_){
            SDL_RenderCopy( win_->getRenderer(), sdlTexture_, NULL, &rect_);
        }
    }
};


class SDLTextView : public SDLView{
protected:
    TTF_Font * font_ = NULL;
    SDL_Surface * surface_ = NULL;
    std::string text_;
    SDL_Rect rect_;
    
    void freeImage(){
        if(sdlTexture_){
            SDL_DestroyTexture(sdlTexture_);
            sdlTexture_ = NULL;
        }
        
        if(surface_){
            SDL_FreeSurface(surface_);
            surface_ = NULL;
        }
    }
public:
    SDLTextView(){
        font_ = TTF_OpenFont("/Library/Fonts/Arial.ttf", 20);
    }
    
    virtual ~SDLTextView(){
        this->freeImage();
        if(font_){
            TTF_CloseFont(font_);
            font_ = NULL;
        }
    }
    
    int draw(const std::string& text){
        this->freeImage();
        text_ = text;
        if(win_){
            win_->refresh();
        }
        return 0;
    }
    
    virtual void onDraw()override{
        if(!sdlTexture_ && win_){
            SDL_Color color = { 255, 0, 0 };
            surface_ = TTF_RenderText_Solid(font_, text_.c_str(), color);
            sdlTexture_ = SDL_CreateTextureFromSurface(win_->getRenderer(), surface_);
            rect_.x = x_;
            rect_.y = y_;
            SDL_QueryTexture(sdlTexture_, NULL, NULL, &rect_.w, &rect_.h);
            //TTF_SizeUTF8(font_, text_.c_str(), &rect_.w, &rect_.h);
        }
        if(sdlTexture_ && win_){
            SDL_RenderCopy( win_->getRenderer(), sdlTexture_, NULL, &rect_);
        }
    }
};

class SDLVideoView : public SDLView{
public:
    void draw(int w, int h, const Uint8 * data){
        this->drawYUV(w, h
                , data+0,   w
                , data+w*h, w/2
                , data+w*h*5/4, w/2);
    }
    void drawYUV(int w, int h
                 , const Uint8 *Yplane, int Ypitch
                 , const Uint8 *Uplane, int Upitch
                 , const Uint8 *Vplane, int Vpitch){
        if(sdlTexture_){
            int tw = 0;
            int th = 0;
            SDL_QueryTexture(sdlTexture_, NULL, NULL, &tw, &th);
            if(tw != w || th != h){
                SDL_DestroyTexture(sdlTexture_);
                sdlTexture_ = NULL;
            }
        }
        
        if(!sdlTexture_ && win_){
            sdlTexture_ = SDL_CreateTexture(win_->getRenderer(), SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, w, h);
        }
        
        if(sdlTexture_){
            SDL_UpdateYUVTexture(sdlTexture_, NULL, Yplane, Ypitch, Uplane, Upitch, Vplane, Vpitch);
        }
        
        if(win_){
            win_->refresh();
        }
    }
    
    virtual void onDraw()override{
        
        if(sdlTexture_ && win_){
            SDL_RenderCopy( win_->getRenderer(), sdlTexture_, NULL, NULL);
        }
    }
};

class SDLAudioPlayer{
    SDL_AudioSpec audioSpec_;
    SDL_AudioDeviceID devId_ = 0;
public:
    virtual ~SDLAudioPlayer(){
        this->close();
    }
    
    int getFrameSize(){
        return audioSpec_.samples;
    }
    
    int getChannels(){
        return audioSpec_.channels;
    }
    
    int64_t getTimeMsOfSamples(int64_t samplesPerCh){
        return 1000*samplesPerCh/audioSpec_.freq;
    }
    
    int open(int samplerate, int channels, int samplesPerCh){
        int ret = -1;
        do {
            this->close();
            SDL_AudioSpec& wanted_spec = audioSpec_;
            wanted_spec.freq = samplerate;
            wanted_spec.format = AUDIO_S16SYS;
            wanted_spec.channels = channels;
            wanted_spec.silence = 0;
            wanted_spec.samples = samplesPerCh;
            wanted_spec.callback = NULL;
            wanted_spec.userdata = NULL;
            
            // 打开音频播放设备
            devId_ = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, NULL, 0);
            if(devId_ == 0){
                //odbge("can't open SDL audio");
                ret = -11;
                break;
            }
            ret = 0;
        } while (0);
        if(ret){
            this->close();
        }
        return ret;
    }
    
    void close(){
        if(devId_ > 0){
            SDL_CloseAudioDevice(devId_);
            devId_ = 0;
        }
    }
    
    int play(bool playing){
        SDL_PauseAudioDevice(devId_, playing?0:1);
        return 0;
    }
    
    int queueSamples(int16_t sampleData[], int sampleCount){
        return SDL_QueueAudio(devId_, sampleData, sampleCount*sizeof(int16_t));
    }
    
    int64_t getRemainTimeMs(){
        uint32_t remains = SDL_GetQueuedAudioSize(devId_);
        return this->getTimeMsOfSamples(remains/sizeof(int16_t)/this->getChannels());
    }
    
    void waitForComplete(){
        int64_t ms = this->getRemainTimeMs();
        while(ms > 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            ms = this->getRemainTimeMs();
        }
    }
};

class SDL{
public:
    static void FlushEvent();
};

#endif /* SDLFramework_hpp */
