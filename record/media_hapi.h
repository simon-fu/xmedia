#ifndef _MEDIA_HAPI_H_
#define _MEDIA_HAPI_H_
#define MEDIAEXPORT __attribute__((visibility("default")))


// simon
// api header file
#include <inttypes.h>
#ifdef __cplusplus
extern "C"{
#endif
    void rprintlogf ( const char * format, ... );
    
    typedef enum{
        MCODEC_ID_NONE = 0,
        MCODEC_ID_UNKNOWN,
        MCODEC_ID_YUV420P,
        MCODEC_ID_VP8,
        MCODEC_ID_VP9,
        MCODEC_ID_H264,
        MCODEC_ID_PCM,
        MCODEC_ID_OPUS,
        MCODEC_ID_ILBC,
        MCODEC_ID_ISAC,
        MCODEC_ID_G722,
        MCODEC_ID_MAX
    }mediacodec_id_t;
    
    /**
	*Media data information beginning with callback
	*context 
	*stream_id         :stream id
	*video_codec_id    :video codec id
	*video_width       :video width
	*video_height      :video height
	*audio_codec_id    :audio codec id
	*audio_samplerate  :audio sampling rate
	*audio_channels    :audio channels
	*playout_codec_id  :playout codec id
	*playout_samplerate:playout samplerate
	*playout_channels  :playout channels
	*ppstream_context  
	*
    **/
    typedef int(*media_callback_remote_start_t)(void * context, int stream_id
    , int video_codec_id, int video_width, int video_height
    , int audio_codec_id, int audio_samplerate, int audio_channels
    , int playout_codec_id, int playout_samplerate, int playout_channels
    , void ** ppstream_context);
     /**
	*Remote video data callback stop
	*context 
	*stream_id         :stream id
	*
    **/
    typedef void(*media_callback_remote_stop_t)(void * stream_context, int stream_id);
    /**
	*Function type of callback encoding after media data
	*stream_context 
	*stream_id         :stream id
	*pts               :time stamp
	*data              :media data
	*data_length       :data length
    **/
    typedef int(*media_callback_media_data_t)(void * stream_context, int stream_id, int codec, int64_t pts,  unsigned char * data, int data_length );
    /**
	*Function type of callback video raw data
	*stream_context 
	*stream_id         :stream id
	*codec             :video type,default is yuv420p
	*pts               :time stamp
	*width             :video width
	*height            :video height
	*rotation          :Video rotation angle
	*datas[]           :video data
	*strides[]         :length of each video plane
	*data_count        :number of plane
    **/
    typedef int(*media_callback_video_raw_t)(void * stream_context, int stream_id, int codec, int64_t pts,  int width, int height, int rotation
    , unsigned char * datas[], int strides[], int data_count );
    
    typedef int (*media_callback_stream_start_t)(void * context, int stream_id, void ** ppstream_context);
    
    typedef void(*media_callback_stream_stop_t)(void * stream_context, int stream_id);
    
    
    //    typedef void(*media_callback_pcm_t)(void * context, int stream_id, int sample_rate, int channels, int64_t pts, unsigned char * data, int data_length );
    /**
	*Registered remote callback interface
	*cb_start
	*cb_stop
	*cb_video
	*cb_audio
	*cb_pcm
	*cb_videoraw  
    **/
    MEDIAEXPORT int hapi_register_remote(void * context
                                         , media_callback_remote_start_t cb_start
                                         , media_callback_remote_stop_t  cb_stop
                                         , media_callback_media_data_t   cb_video
                                         , media_callback_media_data_t   cb_audio
                                         , media_callback_media_data_t   cb_pcm
                                         , media_callback_video_raw_t    cb_videoraw
    // , media_callback_video_raw_t    cb_localvideoraw
    );
    
    MEDIAEXPORT int hapi_register_local_startstop(media_callback_stream_start_t  cb_local_start
                                                  , media_callback_stream_stop_t cb_local_stop);
    MEDIAEXPORT int hapi_local_audioraw_sample_rate(int stream_id);
    MEDIAEXPORT int hapi_local_audioraw_channels(int stream_id);
    /**
	*Registered local videoraw callback interface
    **/
    MEDIAEXPORT int hapi_register_local_videoraw(media_callback_video_raw_t cb_local_videoraw);
    
    // cb_local_audioraw should return data_length
    MEDIAEXPORT int hapi_register_local_audioraw(media_callback_media_data_t cb_local_audioraw);
    MEDIAEXPORT void RequestKeyFrame();


    typedef int (* media_callback_local_platform_video_t) (void * context, void * obj, void * opaqueBuffer);
    MEDIAEXPORT void hapi_register_local_platform_video(void * context, media_callback_local_platform_video_t callback);
    MEDIAEXPORT void hapi_register_input_local_video(void * context, void * obj, size_t width, size_t height, int format, uint8_t * frameData, size_t frameSize, int rotation);

    
#ifdef __cplusplus
} // extern "C"{
#endif



#endif

