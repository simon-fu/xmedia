

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

extern "C"
{
#include <SDL/SDL.h>

};


#define dbgd(...) do{  printf("[DEBUG] " __VA_ARGS__); printf("\n");}while(0)
#define dbgi(...) do{  printf("[INFO ] " __VA_ARGS__); printf("\n");}while(0)
#define dbgw(...) do{  printf("[WARN ] " __VA_ARGS__); printf("\n");}while(0)
#define dbge(...) do{  printf("[ERROR] " __VA_ARGS__); printf("\n");}while(0)


// #include"SDL-1.2.14\include\SDL.h"

/* The pre-defined attributes of the screen */
// CIF

static const char * YUV_FILENAME = "CiscoVT2people_320x192_12fps.yuv";
const int SCREEN_WIDTH  = 320;
const int SCREEN_HEIGHT = 192;
const int SCREEN_BPP    = 8;

typedef struct yuv_pic
{
  char* pv_data[4];
  int   v_linesize[4]; //number of bytes per line
} s_picture;

int main( int argc, char* args[])
{
  FILE* pf_yuv;
  SDL_Surface* ps_screen;
  SDL_Overlay* ps_bmp;
  SDL_Rect     s_rect;

  char*        pv_frame = NULL;
  int          v_pic_size = 0, v_frame_size = 0;
  s_picture    s_yuv_pic;

  /* Init SDL */
  if( SDL_Init(SDL_INIT_EVERYTHING) == -1)
  {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }

  /* Set up the screen */
  ps_screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 0, SDL_SWSURFACE);
  if( !ps_screen )
  {
    fprintf(stderr, "SDL: could not set video mode -exiting\n");
    exit(1);
  }

  /* Set the window caption */
  SDL_WM_SetCaption( "YUV Window", NULL);

  /* Create a YUV overlay */
  ps_bmp = SDL_CreateYUVOverlay(SCREEN_WIDTH, SCREEN_HEIGHT, SDL_YV12_OVERLAY,ps_screen);

  /* Open the YUV file: coastguard_cif.yuv 为测试文件 */
  if( (pf_yuv = fopen(YUV_FILENAME, "rb")) == NULL )
  {
    fprintf(stderr, "SYS: could not open file %s\n", "akiyo_cif.yuv");
    exit(2);
  }

  v_frame_size = (SCREEN_WIDTH * SCREEN_HEIGHT);
  v_pic_size   = ((v_frame_size * 3) / 2);
  if( (pv_frame = (char*)calloc(v_pic_size, 1)) == NULL )
  {
    fprintf(stderr, "SYS: could not allocate mem space\n");
    exit(2);
  }

  s_yuv_pic.pv_data[0] = pv_frame;  //Y
  s_yuv_pic.pv_data[1] = (char*)(pv_frame + v_frame_size);
  s_yuv_pic.pv_data[2] = (char*)(pv_frame + v_frame_size + v_frame_size/4);
  s_yuv_pic.v_linesize[0] = SCREEN_WIDTH;
  s_yuv_pic.v_linesize[1] = (SCREEN_WIDTH/2);
  s_yuv_pic.v_linesize[2] = (SCREEN_WIDTH/2);

  /* Apply the image to the screen */
  ps_bmp->pixels[0] = (unsigned char*)(s_yuv_pic.pv_data[0]);
  ps_bmp->pixels[2] = (unsigned char*)(s_yuv_pic.pv_data[1]);
  ps_bmp->pixels[1] = (unsigned char*)(s_yuv_pic.pv_data[2]);

  ps_bmp->pitches[0] = s_yuv_pic.v_linesize[0];
  ps_bmp->pitches[2] = s_yuv_pic.v_linesize[1];
  ps_bmp->pitches[1] = s_yuv_pic.v_linesize[2];

  /* Update the screen */
  s_rect.x = 0;
  s_rect.y = 0;
  s_rect.w = SCREEN_WIDTH+8;
  s_rect.h = SCREEN_HEIGHT+8;

  /* Load YUV frames */
  int i = 0, ret=0;
  while( 1 )
  {
    ret = fread(pv_frame, 1, v_pic_size, pf_yuv);
    if(ret <= 0){
      dbgi("reach file end");
      break;
    }
    i++;
    fprintf(stderr, "%dth frame's ret = %d\n", i, ret);

    fseek(pf_yuv, v_pic_size*i, SEEK_SET);

    SDL_LockYUVOverlay(ps_bmp);
    SDL_DisplayYUVOverlay(ps_bmp, &s_rect);
    SDL_UnlockYUVOverlay(ps_bmp);
    SDL_Delay( 40 );
  }
  fprintf(stderr, "done\n");
  getchar();

  /* Free the surfaces */
  fclose(pf_yuv);
  free(pv_frame);

  /* Quit SDL */
  SDL_Quit();

  return 0;
}





