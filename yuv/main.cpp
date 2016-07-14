

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

extern "C"
{
#include <SDL2/SDL.h>
#include <libyuv/convert.h>
#include <libyuv/convert_from.h>
#include <libyuv/video_common.h>
};


#define dbgd(...) do{  printf("[DEBUG] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{  printf("[INFO ] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgw(...) do{  printf("[WARN ] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{  printf("[ERROR] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)


static
int get_yuv_data_std(int * pwidth, int * pheight, unsigned char * buf, int * plength){
  const char * filename = "CiscoVT2people_320x192_12fps.yuv";
  int pixel_w=320;
  int pixel_h=192;

  FILE *fp=NULL;
  fp=fopen(filename,"rb+");
  if(fp==NULL){
    dbge("cannot open %s\n", filename);
    return -1;
  }
  dbgi("successfully open %s", filename);

  int ret = 0;
  int frame_size = pixel_w * pixel_h * 3/2;
  int bytes = fread(buf, 1, frame_size, fp) ;
  if(bytes != frame_size){
    dbge("read expect %d but %d", frame_size, bytes);
    ret = -1;
  }else{
    // dbgi("read %d bytes", bytes);
    *pwidth = pixel_w;
    *pheight = pixel_h;
    *plength = bytes;
    ret = 0;
  }

  fclose(fp);
  return ret;
}

static
int copy_plane(unsigned char * dst, const unsigned char * src, int stride, int width, int height){
  for(int i = 0 ; i < height; i++){
    memcpy(dst, src, width);
    // memset(dst, 10, width/2);
    src += stride;
    dst += width;
  }
  return width*height;
}


static
int get_yuv_data2(int * pwidth, int * pheight, unsigned char * buf, int * plength){
  const char * filename = "test_320x240_stride_768_bufsize_276480.yuv";
  int pixel_w=320;
  int pixel_h=240;
  
  static int stride_curr = 768; // 768
  int stride = stride_curr;
  // stride_curr-=1;
  int bytes_to_read = 276480;

  // const char * filename = "test_480x640_stride_768_bufsize_737280.yuv";
  // int pixel_w=480;
  // int pixel_h=640;
  
  // static int stride_curr = 768; // 768
  // int stride = stride_curr;
  // // stride_curr-=1;
  // int bytes_to_read = 737280;

  

  FILE *fp=NULL;
  fp=fopen(filename,"rb+");
  if(fp==NULL){
    dbge("cannot open %s\n", filename);
    return -1;
  }
  dbgi("successfully open %s", filename);

  int ret = 0;
  
  unsigned char *temp_buffer = (unsigned char *)malloc(bytes_to_read);
  do{
    fseek(fp, 0*bytes_to_read, SEEK_SET);
    int bytes = fread(temp_buffer, 1, bytes_to_read, fp) ;
    if(bytes != bytes_to_read){
      dbge("read expect %d but %d", bytes_to_read, bytes);
      ret = -1;
      break;
    }
    dbgi("read %d bytes, stride=%d", bytes, stride);



    int uv_stride = stride / 2;
    int uv_width = pixel_w / 2;
    int uv_height = pixel_h / 2;
    int uv_size = pixel_w * pixel_h / 4;

    int src_pos = 0;
    int dst_pos = 0;

    // Y 
    copy_plane(buf+dst_pos, temp_buffer+src_pos, stride, pixel_w, pixel_h);
    dst_pos += pixel_w * pixel_h;
    src_pos += stride * pixel_h;

    // int clrsize = pixel_w*pixel_h/2;
    // memset(buf, 0, clrsize);
    // dbgi("clear size %d", clrsize);
    
    // // U 
    // copy_plane(buf+dst_pos, temp_buffer+src_pos, uv_stride, uv_width, uv_height);
    // // memset(buf+dst_pos, -127, uv_size);
    // dst_pos += uv_size;
    // src_pos += stride * pixel_h / 4;

    // // V
    // copy_plane(buf+dst_pos, temp_buffer+src_pos, uv_stride, uv_width, uv_height);
    // // memset(buf+dst_pos, -127, uv_size);
    // dst_pos += uv_size;
    // src_pos += stride * pixel_h / 4;

    // clear U V
    memset(buf+dst_pos, 0x81, uv_size); // 0x81 == -127
    dst_pos += uv_size;
    memset(buf+dst_pos, -127, uv_size); // 0x81 == -127
    dst_pos += uv_size;
    
    *pwidth = pixel_w;
    *pheight = pixel_h;
    *plength = dst_pos;
    ret = 0;

  }while(0);


  free(temp_buffer);
  fclose(fp);
  return ret;
}

static
int get_yuv_data3(int * pwidth, int * pheight, unsigned char * buf, int * plength){
  // const char * filename = "test_320x240_stride_768_bufsize_276480.yuv";
  // int pixel_w=320;
  // int pixel_h=240;
  
  // static int stride_curr = 768; // 768
  // int stride = stride_curr;
  // // stride_curr-=1;
  // int bytes_to_read = 276480;

  const char * filename = "test_480x640_stride_768_bufsize_737280.yuv";
  int pixel_w=480;
  int pixel_h=640;
  
  static int stride_curr = 768; // 768
  int stride = stride_curr;
  // stride_curr-=1;
  int bytes_to_read = 737280;

  

  FILE *fp=NULL;
  fp=fopen(filename,"rb+");
  if(fp==NULL){
    dbge("cannot open %s\n", filename);
    return -1;
  }
  dbgi("successfully open %s", filename);

  int ret = 0;
  
  unsigned char *temp_buffer = (unsigned char *)malloc(bytes_to_read);
  do{
    fseek(fp, 0*bytes_to_read, SEEK_SET);
    int bytes = fread(temp_buffer, 1, bytes_to_read, fp) ;
    if(bytes != bytes_to_read){
      dbge("read expect %d but %d", bytes_to_read, bytes);
      ret = -1;
      break;
    }
    dbgi("read %d bytes, stride=%d", bytes, stride);



    int uv_stride = stride / 2;
    int uv_width = pixel_w / 2;
    int uv_height = pixel_h / 2;
    int uv_size = pixel_w * pixel_h / 4;

    unsigned char * y_ptr = temp_buffer;
    int y_bigsize = stride*pixel_h;

    unsigned char * u_ptr = y_ptr + y_bigsize;
    int u_bigsize = uv_stride * uv_height;

    unsigned char * v_ptr = u_ptr + u_bigsize;
    int v_bigsize = uv_stride * uv_height;

    memset(u_ptr, 0x81, u_bigsize); // 0x81 == -127
    memset(v_ptr, 0x81, v_bigsize); // 0x81 == -127

    // I411ToI420   I420Copy
    libyuv::I420Copy(y_ptr, stride,
                     u_ptr, uv_stride,
                     v_ptr, uv_stride,
                     buf,  // y ptr
                     pixel_w, // y stride
                     buf+pixel_w*pixel_h, // u ptr
                     pixel_w/2, // u stride
                     buf+pixel_w*pixel_h*5/4, // v ptr
                     pixel_w/2, // v stride
                     pixel_w, pixel_h);


    // int src_pos = 0;
    // int dst_pos = 0;

    // // Y 
    // copy_plane(buf+dst_pos, temp_buffer+src_pos, stride, pixel_w, pixel_h);
    // dst_pos += pixel_w * pixel_h;
    // src_pos += stride * pixel_h;

    // // int clrsize = pixel_w*pixel_h/2;
    // // memset(buf, 0, clrsize);
    // // dbgi("clear size %d", clrsize);
    
    // // U 
    // copy_plane(buf+dst_pos, temp_buffer+src_pos, uv_stride, uv_width, uv_height);
    // // memset(buf+dst_pos, -127, uv_size);
    // dst_pos += uv_size;
    // src_pos += stride * pixel_h / 4;

    // // V
    // copy_plane(buf+dst_pos, temp_buffer+src_pos, uv_stride, uv_width, uv_height);
    // // memset(buf+dst_pos, -127, uv_size);
    // dst_pos += uv_size;
    // src_pos += stride * pixel_h / 4;

    // // clear U V
    // memset(buf+dst_pos, 0x81, uv_size); // 0x81 == -127
    // dst_pos += uv_size;
    // memset(buf+dst_pos, -127, uv_size); // 0x81 == -127
    // dst_pos += uv_size;
    
    *pwidth = pixel_w;
    *pheight = pixel_h;
    *plength = pixel_w*pixel_h*3/2;
    ret = 0;

  }while(0);


  free(temp_buffer);
  fclose(fp);
  return ret;
}


static
int get_yuv_data4(int * pwidth, int * pheight, unsigned char * buf, int * plength){
  // const char * filename = "test_320x240_stride_768_bufsize_276480.yuv";
  // int pixel_w=320;
  // int pixel_h=240;
  
  // static int stride_curr = 768; // 768
  // int stride = stride_curr;
  // // stride_curr-=1;
  // int bytes_to_read = 276480;

  const char * filename = "test_640x480_stride_768_bufsize_552960.yuv";
  int pixel_w=640;
  int pixel_h=480;
  
  static int stride_curr = 768; // 768
  int stride = stride_curr;
  // stride_curr-=1;
  int bytes_to_read = 552960;

  

  FILE *fp=NULL;
  fp=fopen(filename,"rb+");
  if(fp==NULL){
    dbge("cannot open %s\n", filename);
    return -1;
  }
  dbgi("successfully open %s", filename);

  int ret = 0;
  
  unsigned char *temp_buffer = (unsigned char *)malloc(bytes_to_read);
  do{
    fseek(fp, 0*bytes_to_read, SEEK_SET);
    int bytes = fread(temp_buffer, 1, bytes_to_read, fp) ;
    if(bytes != bytes_to_read){
      dbge("read expect %d but %d", bytes_to_read, bytes);
      ret = -1;
      break;
    }
    dbgi("read %d bytes, stride=%d", bytes, stride);



    int uv_stride = stride / 2;
    int uv_width = pixel_w / 2;
    int uv_height = pixel_h / 2;
    int uv_size = pixel_w * pixel_h / 4;

    unsigned char * y_ptr = temp_buffer;
    int y_bigsize = stride*pixel_h;

    unsigned char * u_ptr = y_ptr + y_bigsize;
    int u_bigsize = uv_stride * uv_height;

    unsigned char * v_ptr = u_ptr + u_bigsize;
    int v_bigsize = uv_stride * uv_height;

    // memset(u_ptr, 0x81, u_bigsize); // 0x81 == -127
    // memset(v_ptr, 0x81, v_bigsize); // 0x81 == -127

    dbgi("libyuv ...");
    // I411ToI420   I420Copy
    libyuv::I420Copy(y_ptr, stride,
                     u_ptr, uv_stride,
                     v_ptr, uv_stride,
                     buf,  // y ptr
                     pixel_w, // y stride
                     buf+pixel_w*pixel_h, // u ptr
                     pixel_w/2, // u stride
                     buf+pixel_w*pixel_h*5/4, // v ptr
                     pixel_w/2, // v stride
                     pixel_w, pixel_h);

    // NV12ToI420  NV21ToI420
    // libyuv::NV21ToI420(y_ptr, stride,
    //                  u_ptr, stride,
    //                  buf,  // y ptr
    //                  pixel_w, // y stride
    //                  buf+pixel_w*pixel_h, // u ptr
    //                  pixel_w/2, // u stride
    //                  buf+pixel_w*pixel_h*5/4, // v ptr
    //                  pixel_w/2, // v stride
    //                  pixel_w, pixel_h);

    dbgi("libyuv done");

    // int src_pos = 0;
    // int dst_pos = 0;

    // // Y 
    // copy_plane(buf+dst_pos, temp_buffer+src_pos, stride, pixel_w, pixel_h);
    // dst_pos += pixel_w * pixel_h;
    // src_pos += stride * pixel_h;

    // // int clrsize = pixel_w*pixel_h/2;
    // // memset(buf, 0, clrsize);
    // // dbgi("clear size %d", clrsize);
    
    // // U 
    // copy_plane(buf+dst_pos, temp_buffer+src_pos, uv_stride, uv_width, uv_height);
    // // memset(buf+dst_pos, -127, uv_size);
    // dst_pos += uv_size;
    // src_pos += stride * pixel_h / 4;

    // // V
    // copy_plane(buf+dst_pos, temp_buffer+src_pos, uv_stride, uv_width, uv_height);
    // // memset(buf+dst_pos, -127, uv_size);
    // dst_pos += uv_size;
    // src_pos += stride * pixel_h / 4;

    // // clear U V
    // memset(buf+dst_pos, 0x81, uv_size); // 0x81 == -127
    // dst_pos += uv_size;
    // memset(buf+dst_pos, -127, uv_size); // 0x81 == -127
    // dst_pos += uv_size;
    
    *pwidth = pixel_w;
    *pheight = pixel_h;
    *plength = pixel_w*pixel_h*3/2;
    ret = 0;

  }while(0);


  free(temp_buffer);
  fclose(fp);
  return ret;
}


static 
int get_yuv_data(int * pwidth, int * pheight, unsigned char * buf, int * plength){
  // return get_yuv_data_std(pwidth, pheight, buf, plength);
  return get_yuv_data4(pwidth, pheight, buf, plength);
}


//Refresh Event
#define REFRESH_EVENT  (SDL_USEREVENT + 1)
//Break
#define BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit=0;

int refresh_video(void *opaque){
  thread_exit=0;
  while (!thread_exit) {
    SDL_Event event;
    event.type = REFRESH_EVENT;
    SDL_PushEvent(&event);
    SDL_Delay(40);
  }
  thread_exit=0;
  //Break
  SDL_Event event;
  event.type = BREAK_EVENT;
  SDL_PushEvent(&event);
  return 0;
}


int main(int argc, char* argv[])
{
static unsigned char buffer[3*1024*1024];
static unsigned char buffer_convert[3*1024*1024];

  int pixel_w = 0;
  int pixel_h = 0;
  int datalength = 0;
  int ret = get_yuv_data(&pixel_w, &pixel_h, buffer, &datalength);
  if(ret < 0){
    return ret;
  }
  dbgi("yuv: w=%d, h=%d, length=%d", pixel_w, pixel_h, datalength);

  int screen_w=pixel_w;
  int screen_h=pixel_h;

  if(SDL_Init(SDL_INIT_VIDEO)) {  
    printf( "Could not initialize SDL - %s\n", SDL_GetError()); 
    return -1;
  } 

  SDL_Window *screen; 
  //SDL 2.0 Support for multiple windows
  screen = SDL_CreateWindow("Simplest Video Play SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    screen_w, screen_h,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
  if(!screen) {  
    printf("SDL: could not create window - exiting:%s\n",SDL_GetError());  
    return -1;
  }
  SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen, -1, 0);  

  Uint32 pixformat=0;
  //IYUV: Y + U + V  (3 planes)
  //YV12: Y + V + U  (3 planes)
  pixformat= SDL_PIXELFORMAT_IYUV; 

  SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer,pixformat, SDL_TEXTUREACCESS_STREAMING,pixel_w,pixel_h);


  SDL_Rect sdlRect;  

  SDL_Thread *refresh_thread = SDL_CreateThread(refresh_video,NULL,NULL);
  SDL_Event event;

    SDL_UpdateTexture( sdlTexture, NULL, buffer, pixel_w);
      sdlRect.x = 0;  
      sdlRect.y = 0;  
      sdlRect.w = screen_w;  
      sdlRect.h = screen_h;  
      
      SDL_RenderClear( sdlRenderer );   
      SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, &sdlRect);  
      SDL_RenderPresent( sdlRenderer );  

  while(1){
    //Wait
    SDL_WaitEvent(&event);
    if(event.type==REFRESH_EVENT){
  

        // int ret = get_yuv_data(&pixel_w, &pixel_h, buffer, &datalength);
        // if(ret < 0){
        //   return ret;
        // }
        // dbgi("yuv: w=%d, h=%d, length=%d", pixel_w, pixel_h, datalength);
  
      

      
    }else if(event.type==SDL_WINDOWEVENT){
      //If Resize
      SDL_GetWindowSize(screen,&screen_w,&screen_h);
    }else if(event.type==SDL_QUIT){
      thread_exit=1;
    }else if(event.type==BREAK_EVENT){
      break;
    }
  }
  SDL_Quit();
  
  return 0;
}





