#define SDL_STBIMAGE_IMPLEMENTATION
#define SDL_STBIMG_ALLOW_STDIO
//#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "Lab020-Beauty.hpp"
#include "SDLFramework.hpp"
#include "FFMpegFramework.hpp"
#include "rbf.hpp"

//#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
//#include "rbf.hpp"
#include <stdio.h>
#include <time.h>

#include "SDL_stb/SDL_stbimage.h"

#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)


class Timer {
private:
    unsigned long begTime;
public:
    void start() { begTime = clock(); }
    float elapsedTime() { return float((unsigned long)clock() - begTime) / CLOCKS_PER_SEC; }
};

class YUVFileReader{
    std::string filename_;
    int width_;
    int height_;
    int num_ = 0;
    FILE * fp_ = NULL;
    int imageBufSize_ = 0;
    uint8_t * imageBuf_ = NULL;
public:
    YUVFileReader(std::string& filename, int width, int height)
    : filename_(filename)
    , width_(width)
    , height_(height){
        
    }
    
    virtual ~YUVFileReader(){
        this->close();
    }
    
    int open(){
        int ret = -1;
        do{
            this->close();
            fp_ = fopen(filename_.c_str(), "rb");
            if (!fp_) {
                odbge("fail to open yuv file [%s]", filename_.c_str());
                ret = -1;
                break;
            }
            
            int yuvBitsPerPixel = 12;
            imageBufSize_ = width_ * height_ * yuvBitsPerPixel/8;
            imageBuf_ = new uint8_t[imageBufSize_];
            ret = 0;
        }while(0);
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
        if(imageBuf_){
            delete[] imageBuf_;
            imageBuf_ = NULL;
        }
        num_ = 0;
    }
    
    void rewind(){
        if(fp_){
            fseek(fp_, 0, SEEK_SET);
            num_ = 0;
        }
    }
    
    const uint8_t * read(){
        const uint8_t * data = NULL;
        if(fp_){
            size_t bytes = fread(imageBuf_, sizeof(char), imageBufSize_, fp_);
            if(bytes == imageBufSize_){
                data = imageBuf_;
            }
        }
        return data;
    }
    
    const uint8_t * readAt(int frameNo){
        if(fp_){
            long offset = frameNo * imageBufSize_;
            int ret = fseek(fp_, offset, SEEK_SET);
            if(ret == 0){
                return read();
            }
        }
        return NULL;
    }
    
    int getImageLength(){
        return imageBufSize_;
    }
    
    int getImageWidth(){
        return width_;
    }
    
    int getImageHeight(){
        return height_;
    }
};

class CameraBeauty{
    const std::string deviceName = "FaceTime HD Camera"; // or "2";
    const std::string optFormat = "avfoundation";
    const std::string optPixelFmt; // = "yuyv422";
    int width_ = 640;
    int height_ = 480;
    int frameRate_ = 30;
    float sigma_spatial_ = 0.03f;
    float sigma_range_ = 0.05f; //0.1f;
    
public:
    
    void drawText(SDLTextView * textView, bool isBeauty){
        char buf[64];
        sprintf(buf, "press SPACE to switch beauty = %d, s=%.3f, r=%.3f", isBeauty, sigma_spatial_, sigma_range_);
        textView->draw(buf);
    }
    int run(){
        int ret = -1;
        FFContainerReader * deviceReader = new FFContainerReader("dev");
        SDLWindowImpl * window = NULL;
        unsigned char * img_data_out = 0;
        do{
            deviceReader = new FFContainerReader("dev");
            deviceReader->setVideoOptions(width_, height_, frameRate_, optPixelFmt);
            ret = deviceReader->open(optFormat, deviceName);
            if(ret){
                odbge("fail to open camera, ret=%d", ret);
                break;
            }
            //FFVideoTrack * videoTrack = deviceReader->openVideoTrack(AV_PIX_FMT_YUV420P);
            FFVideoTrack * videoTrack = deviceReader->openVideoTrack(AV_PIX_FMT_RGB24);
            if(!videoTrack){
                odbge("fail to open video track");
                ret = -22;
                break;
            }
            
            bool isBeauty = true;
            window = new SDLWindowImpl("camera beauty", width_, height_);
            window->open();
            SDLSurfaceView * surfaceView = NULL;
            surfaceView = new SDLSurfaceView();
            window->addView(surfaceView);
            SDLTextView * textView = new SDLTextView();
            window->addView(textView);
            drawText(textView, isBeauty);
            
            AVFrame * avframe = NULL;
            SDL_Event event;
            bool quitLoop = false;
            while(!quitLoop){
                FFTrack * track = deviceReader->readNext();
                if(track->getMediaType() == AVMEDIA_TYPE_VIDEO){
                    avframe = track->getLastFrame();
                    if(avframe){
                        odbgi("camera image: w=%d, h=%d, size=%d, fmt=%s"
                              , avframe->width, avframe->height, avframe->pkt_size, av_get_pix_fmt_name((AVPixelFormat)avframe->format));
                    }
                    
                    int bytesPerPixel =  STBI_rgb ;
                    unsigned char * imgData = avframe->data[0];
                    if(isBeauty){
                        
                        recursive_bf(avframe->data[0], img_data_out, sigma_spatial_, sigma_range_, avframe->width, avframe->height, bytesPerPixel);
                        imgData = img_data_out;
                    }
                    
                    SDL_Surface* surf = STBIMG_CreateSurface(imgData, avframe->width, avframe->height, bytesPerPixel, SDL_FALSE);
                    surfaceView->draw(surf);
                    
                }
                
                while (SDL_PollEvent(&event)) {
                    if(event.type==SDL_QUIT){
                        odbgi("got QUIT event %d", event.type);
                        quitLoop = true;
                        break;
                    }else if(event.type == SDL_KEYDOWN){
                        odbgi("got keydown event %d, win=%d, key=%d, scan=%d, mod=%d", event.type, event.key.windowID, event.key.keysym.sym, event.key.keysym.scancode, event.key.keysym.mod);
                        if(event.key.keysym.sym == SDLK_SPACE){
                            isBeauty = !isBeauty;
                            drawText(textView, isBeauty);
                        }else if(event.key.keysym.sym == SDLK_s){
                            if(event.key.keysym.mod & KMOD_CTRL){
                                sigma_spatial_ = sigma_spatial_ - 0.01f;
                            }else{
                                sigma_spatial_ = sigma_spatial_ + 0.01f;
                            }
                            drawText(textView, isBeauty);
                        }else if(event.key.keysym.sym == SDLK_r){
                            if(event.key.keysym.mod & KMOD_CTRL){
                                sigma_range_ = sigma_range_ - 0.01f;
                            }else{
                                sigma_range_ = sigma_range_ + 0.01f;
                            }
                            drawText(textView, isBeauty);
                        }
                    }
                }
                
            }
            ret = 0;
        }while(0);
        
        if(deviceReader){
            delete deviceReader;
            deviceReader = NULL;
        }
        
        if(img_data_out){
            delete [] img_data_out;
            img_data_out = NULL;
        }
        
        return ret;
    }
    
};

int lab020_beauty_main(int argc, char* argv[]){
    int ret = SDL_Init(SDL_INIT_VIDEO) ;
    if(ret){
        odbge( "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
    ret = TTF_Init();
    av_register_all();
    avformat_network_init();
    avdevice_register_all();
    
    CameraBeauty * beauty = new CameraBeauty();
    beauty->run();
    delete beauty;
    
    TTF_Quit();
    SDL_Quit();
    return 0;
}

int lab020_beauty_main0(int argc, char* argv[]){
    int ret = SDL_Init(SDL_INIT_VIDEO) ;
    if(ret){
        odbge( "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
    av_register_all();
    avformat_network_init();
    avdevice_register_all();
    
    const char * imageFilePath = "/Users/simon/Downloads/face.ppm";
    SDL_Surface* surf = STBIMG_Load(imageFilePath);
    if(surf == NULL) {
        odbge("ERROR: Couldn't load %s, reason: %s\n", imageFilePath, SDL_GetError());
        exit(1);
    }
    
    int width = 0;
    int height = 0;
    
    float sigma_spatial = 0.03f;
    float sigma_range = 0.1f;
    int channel = 0;
    unsigned char * img_data = stbi_load(imageFilePath, &width, &height, &channel, 0);
    odbgi("img: size=%dx%d, ch=%d", width, height, channel);
    
    unsigned char * img_data_out = 0;
    recursive_bf(img_data, img_data_out, sigma_spatial, sigma_range, width, height, channel);
    int bytesPerPixel = (channel == STBI_grey || channel == STBI_rgb) ? STBI_rgb : STBI_rgb_alpha;
    SDL_bool freeWithSurface = SDL_TRUE;
    SDL_Surface* surf2 = STBIMG_CreateSurface(img_data_out, width, height, bytesPerPixel, freeWithSurface);
    if(freeWithSurface){
        img_data_out = NULL;
    }
    
//    std::string filepath = "/Users/simon/easemob/src/xmedia/LearningRTC/data/test_yuv420p_640x360.yuv";
//    YUVFileReader * yuvreader = NULL;
//    yuvreader = new YUVFileReader(filepath, 640, 360);
//    ret = yuvreader->open();
//    const uint8_t * yuvdata = yuvreader->readAt(30);
//    SDLVideoView * videoView = new SDLVideoView();
    

    
    SDLWindowImpl * window = NULL;
    SDLSurfaceView * surfaceView = NULL;
    surfaceView = new SDLSurfaceView();
    surfaceView->draw(surf2);
    surfaceView->getImageSize(width, height);
    odbgi("surf: size=%dx%d", width, height);
    
    window = new SDLWindowImpl("stb image", width, height);
    window->open();
    window->addView(surfaceView);
//    window->addView(videoView);
//    videoView->draw(yuvreader->getImageWidth(), yuvreader->getImageHeight(), yuvdata);
    
    SDL_Event event;
    bool quitLoop = false;
    while(!quitLoop){
        SDL_WaitEvent(&event);
        if(event.type==SDL_QUIT){
            odbgi("got QUIT event %d", event.type);
            quitLoop = true;
            break;
        }
    }
    
    delete window;
    if(img_data_out){
        delete[] img_data_out;
        img_data_out = NULL;
    }
    if(img_data){
        delete[] img_data;
        img_data = NULL;
    }
//    if(yuvreader){
//        delete yuvreader;
//        yuvreader = NULL;
//    }

    
    SDL_Quit();
    return ret;
    
    
//    if (argc != 5){
//        printf("Usage:\n");
//        printf("--------------------------------------------------------------------\n\n");
//        printf("rbf filename_out filename_in (only support ppm images) \n");
//        printf("    sigma_spatial(e.g., 0.03) sigma_range(e.g., 0.1)\n\n");
//        printf("--------------------------------------------------------------------\n");
//        return(-1);
//    }
//
//    const int n = 100;
//    const char * filename_out = argv[1];
//    const char * filename_in = argv[2];
//    float sigma_spatial = static_cast<float>(atof(argv[3]));
//    float sigma_range = static_cast<float>(atof(argv[4]));
//
//    int width, height, channel;
//    unsigned char * img = stbi_load(filename_in, &width, &height, &channel, 0);
//    unsigned char * img_out = 0;
//    Timer timer;
//
//    timer.start();
//    for (int i = 0; i < n; ++i)
//        recursive_bf(img, img_out, sigma_spatial, sigma_range, width, height, channel);
//    printf("Internal Buffer: %2.5fsecs\n", timer.elapsedTime() / n);
//
//
//    float * buffer = new float[(width * height* channel + width * height
//                                + width * channel + width) * 2];
//    timer.start();
//    for (int i = 0; i < n; ++i)
//        recursive_bf(img, img_out, sigma_spatial, sigma_range, width, height, channel, buffer);
//    printf("External Buffer: %2.5fsecs\n", timer.elapsedTime() / n);
//    delete[] buffer;
//
//    stbi_write_bmp(filename_out, width, height, channel, img_out);
//    delete[] img;
//
//    return  0;
}
