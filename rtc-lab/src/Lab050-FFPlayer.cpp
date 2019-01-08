
#include "Lab050-FFPlayer.hpp"
#include "FFMpegFramework.hpp"
#include "SDLFramework.hpp"
#include "NLogger.hpp"

static auto main_logger = NLogger::Get("main");
//static NObjDumperLog maindumper(*main_logger);
#define odbgd(FMT, ARGS...) main_logger->debug(FMT, ##ARGS)
#define odbgi(FMT, ARGS...) main_logger->info(FMT, ##ARGS)
#define odbge(FMT, ARGS...) main_logger->error(FMT, ##ARGS)
#define DUMPER() NObjDumperLog(*main_logger, NLogger::Level::debug)



#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

//Output PCM
#define OUTPUT_PCM 1
//Use SDL
#define USE_SDL 1

//Buffer:
//|-----------|-------------|
//chunk-------pos---len-----|
static  Uint8  *audio_chunk;
static  Uint32  audio_len;
static  Uint8  *audio_pos;

/* The audio function callback takes the following parameters:
 * stream: A pointer to the audio buffer to be filled
 * len: The length (in bytes) of the audio buffer
 */
void  fill_audio(void *udata,Uint8 *stream,int len){
    //SDL 2.0
    SDL_memset(stream, 0, len);
    if(audio_len==0)        /*  Only  play  if  we  have  data  left  */
        return;
    len=(len>audio_len?audio_len:len);    /*  Mix  as  much  data  as  possible  */
    
    SDL_MixAudio(stream,audio_pos,len,SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
}
//-----------------


int simplest_ffmpeg_audio_player(int argc, char* argv[])
{
    AVFormatContext    *pFormatCtx;
    int                i, audioIndex;
    AVCodecContext    *pCodecCtx;
    AVCodec            *pCodec;
    AVPacket        *packet;
    uint8_t            *out_buffer;
    AVFrame            *pFrame;
    SDL_AudioSpec wanted_spec;
    int ret;
    uint32_t len = 0;
    int got_picture;
    int index = 0;
    int64_t in_channel_layout;
    struct SwrContext *au_convert_ctx;
    
    FILE *pFile=NULL;
    //char url[]="WavinFlag.aac";
    const char *url = "/Users/simon/easemob/src/xmedia/resource/sample.mp4";
    //const char *url = "/Users/simon/Downloads/tmp/tmp/raw-tlv/rtx-fec-02-nodrop.webm";
    
    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();
    //Open
    if(avformat_open_input(&pFormatCtx,url,NULL,NULL)!=0){
        printf("Couldn't open input stream.\n");
        return -1;
    }
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx,NULL)<0){
        printf("Couldn't find stream information.\n");
        return -1;
    }
    // Dump valid information onto standard error
    av_dump_format(pFormatCtx, 0, url, false);
    
    // Find the first audio stream
    audioIndex=-1;
    for(i=0; i < pFormatCtx->nb_streams; i++)
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
            audioIndex=i;
            break;
        }
    
    if(audioIndex==-1){
        printf("Didn't find a audio stream.\n");
        return -1;
    }
    
//    // Get a pointer to the codec context for the audio stream
//    pCodecCtx=pFormatCtx->streams[audioIndex]->codec;
//
//    // Find the decoder for the audio stream
//    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
//    if(pCodec==NULL){
//        printf("Codec not found.\n");
//        return -1;
//    }
//
//    // Open codec
//    if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){
//        printf("Could not open codec.\n");
//        return -1;
//    }

    
    // simon
    AVCodecParameters * codecpar = pFormatCtx->streams[audioIndex]->codecpar;
    pCodec=avcodec_find_decoder(codecpar->codec_id);
    if(pCodec==NULL){
        printf("Codec not found.\n");
        return -1;
    }
    AVCodecContext * codecCtx_ = avcodec_alloc_context3(pCodec);
    pCodecCtx = codecCtx_;
    int channels_ = pFormatCtx->streams[audioIndex]->codecpar->channels;
    int sample_rate_ = pFormatCtx->streams[audioIndex]->codecpar->sample_rate;
    AVDictionary* codecDict = NULL;
    av_dict_set_int(&codecDict, "ac", channels_, 0);
    av_dict_set_int(&codecDict, "ar", sample_rate_, 0);
    if(avcodec_open2(pCodecCtx, pCodec, &codecDict)<0){
        printf("Could not open codec.\n");
        return -1;
    }
    
    
#if OUTPUT_PCM
    pFile=fopen("output.pcm", "wb");
#endif
    
    packet=(AVPacket *)av_malloc(sizeof(AVPacket));
    av_init_packet(packet);
    
    //Out Audio Param
    uint64_t out_channel_layout=AV_CH_LAYOUT_STEREO;
    int out_nb_samples=1024;
    AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;
    int out_sample_rate=44100;
    int out_channels=av_get_channel_layout_nb_channels(out_channel_layout);
    //Out Buffer Size
    int out_buffer_size=av_samples_get_buffer_size(NULL,
                                                   out_channels ,
                                                   out_nb_samples,
                                                   out_sample_fmt,
                                                   1);
    
    out_buffer=(uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE*2);
    pFrame=av_frame_alloc();
    
    //FIX:Some Codec's Context Information is missing
    in_channel_layout=av_get_default_channel_layout(pCodecCtx->channels);
    //Swr
    
    au_convert_ctx = swr_alloc();
    au_convert_ctx=swr_alloc_set_opts(au_convert_ctx,out_channel_layout, out_sample_fmt, out_sample_rate,
                                      in_channel_layout,pCodecCtx->sample_fmt , pCodecCtx->sample_rate,0, NULL);
    swr_init(au_convert_ctx);
    
    //SDL------------------
#if USE_SDL
    //Init
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf( "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
    //SDL_AudioSpec
    wanted_spec.freq = out_sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = out_channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = out_nb_samples;
    wanted_spec.callback = fill_audio;
    wanted_spec.userdata = pCodecCtx;
    
    if (SDL_OpenAudio(&wanted_spec, NULL)<0){
        printf("can't open audio.\n");
        return -1;
    }
#endif
    printf("Bitrate:\t %3lld\n", pFormatCtx->bit_rate);
    printf("Decoder Name:\t %s\n", pCodecCtx->codec->long_name);
    printf("Channels:\t %d\n", pCodecCtx->channels);
    printf("Sample per Second\t %d \n", pCodecCtx->sample_rate);
    
 
    while(av_read_frame(pFormatCtx, packet)>=0){
        if(packet->stream_index==audioIndex){
            
            // simon
            char errbuf[1024];
            ret = avcodec_send_packet(pCodecCtx, packet);
            av_strerror(ret, errbuf, 1024);
            ret = avcodec_receive_frame(pCodecCtx, pFrame);
            av_strerror(ret, errbuf, 1024);
            got_picture = (ret == 0);
            //ret = avcodec_decode_audio4( pCodecCtx, pFrame,&got_picture, packet);
            if ( ret < 0 ) {
                printf("Error in decoding audio frame.\n");
                return -1;
            }
            if ( got_picture > 0 ){
                ret = swr_convert(au_convert_ctx,&out_buffer, MAX_AUDIO_FRAME_SIZE,(const uint8_t **)pFrame->data , pFrame->nb_samples);
                if(ret >= 0){
                    out_buffer_size = ret*out_channels*sizeof(int16_t);
                }
#if 1
                printf("index:%5d\t pts:%lld, pkt-size=%d, out-size=%d\n",index,packet->pts,packet->size,out_buffer_size);
#endif
                
#if USE_SDL
                //FIX:FLAC,MP3,AAC Different number of samples
                if(wanted_spec.samples!=pFrame->nb_samples){
                    SDL_CloseAudio();
                    out_nb_samples=pFrame->nb_samples;
                    out_buffer_size=av_samples_get_buffer_size(NULL,out_channels ,out_nb_samples,out_sample_fmt, 1);
                    wanted_spec.samples=out_nb_samples;
                    SDL_OpenAudio(&wanted_spec, NULL);
                }
#endif
                
#if OUTPUT_PCM
                //Write PCM
                fwrite(out_buffer, 1, out_buffer_size, pFile);
#endif
                
                index++;
                
                //SDL------------------
#if USE_SDL
                //Set audio buffer (PCM data)
                audio_chunk = (Uint8 *) out_buffer;
                //Audio buffer length
                audio_len =out_buffer_size;
                
                audio_pos = audio_chunk;
                //Play
                SDL_PauseAudio(0);
                while(audio_len>0)//Wait until finish
                    SDL_Delay(1);
#endif
            }

        }
        av_free_packet(packet);
    }
    
    swr_free(&au_convert_ctx);
    
#if USE_SDL
    SDL_CloseAudio();//Close SDL
    SDL_Quit();
#endif
    // Close file
#if OUTPUT_PCM
    fclose(pFile);
#endif
    av_free(out_buffer);
    // Close the codec
    avcodec_close(pCodecCtx);
    // Close the video file
    avformat_close_input(&pFormatCtx);
    
    return 0;
}


int lab050_ffplayer_main(int argc, char* argv[]){
    //return simplest_ffmpeg_audio_player(argc, argv);
    
    spdlog::set_pattern("|%H:%M:%S.%e|%n|%L| %v");
    av_register_all();
    avcodec_register_all();
    int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) ;
    if(ret){
        odbge( "fail too init SDL, err=[%s]", SDL_GetError());
        return -1;
    }
    ret = TTF_Init();
    
    //const char * media_filename = "/Users/simon/Downloads/tmp/tmp/raw-tlv/rtx-fec-02-nodrop.webm";
    const char * media_filename = "/Users/simon/easemob/src/xmedia/resource/sample.mp4";
    
    
    
    FFContainerReader * reader = new FFContainerReader("file");
    ret = reader->open("", media_filename);
    
    FFTrack * track = nullptr;
    FFAudioTrack * audio_track = nullptr;
    FFVideoTrack * video_track = nullptr;
    audio_track = reader->openAudioTrack(-1, -1, AV_SAMPLE_FMT_S16);
    //video_track = reader->openVideoTrack(AV_PIX_FMT_YUV420P) ;
    
    SDLAudioPlayer audio_player;
    if(audio_track){
        ret = audio_player.open(audio_track->getSamplerate(),
                          audio_track->getChannels(),
                          1024);
        if(ret){
            odbge("can't open SDL audio, ret=%d", ret);
            //break;
        }
        audio_player.play(true);
    }
    
    
    track = reader->readNext();
    while(track){
        AVFrame * avframe = track->getLastFrame();
        if(avframe){
            if(track == audio_track){
//                int out_size = avframe->nb_samples * avframe->channels * sizeof(int16_t);
                odbgi("frame: ch={}, samples={}, size={}, fmt={}"
                      , avframe->channels, avframe->nb_samples, avframe->pkt_size, av_get_sample_fmt_name((AVSampleFormat)avframe->format));
               ret =  audio_player.queueSamples(avframe->data[0], avframe->pkt_size);
            }else if(track == video_track){
                odbgi("frame: w={}, h={}, size={}, fmt={}"
                      , avframe->width, avframe->height, avframe->pkt_size, av_get_pix_fmt_name((AVPixelFormat)avframe->format));
            }
        }
        track = reader->readNext();
    }
    audio_player.waitForComplete();
    delete reader;
    return 0;
}
