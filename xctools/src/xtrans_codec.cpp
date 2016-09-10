
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <opus/opus.h>
#include <speex/speex.h>
#include "xcutil.h"
#include "xtrans_codec.h"


#define dbgv(...) do{  printf("<xtcodec>[D] " __VA_ARGS__); printf("\n"); }while(0)
#define dbgi(...) do{  printf("<xtcodec>[I] " __VA_ARGS__); printf("\n"); }while(0)
#define dbge(...) do{  printf("<xtcodec>[E] " __VA_ARGS__); printf("\n"); }while(0)



#define OPUS_MAX_FRAM_SIZE 5760

typedef struct opus_encoder_s{
	OpusEncoder * opus_enc;
}opus_encoder_t;

int opus_enc_size(const audio_codec_param * param){
	return TRIM_STRUCT_SIZE(opus_encoder_get_size(param->channels));
}

int opus_enc_init(void * ctx, const audio_codec_param * param){
	OpusEncoder * opus = (OpusEncoder *) ctx;
	int ret = opus_encoder_init(opus, param->sample_rate, param->channels, OPUS_APPLICATION_AUDIO);
	return ret;
}

void opus_enc_destroy(void * ctx){
	OpusEncoder * opus = (OpusEncoder *) ctx;
//	opus_encoder_destroy(opus);
	opus_encoder_ctl(opus, OPUS_RESET_STATE);
}

int opus_enc_frame_range(void * ctx, int *min, int *max, int *recommend){
	OpusEncoder * opus = (OpusEncoder *) ctx;
	int sample_rate;
	opus_encoder_ctl(opus, OPUS_GET_SAMPLE_RATE(&sample_rate));
	*min = 1;
	*max = sample_rate;
	*recommend = 960; //sample_rate/20;
	return 0;
}

int opus_enc_process(void * ctx, const void * in_data, uint32_t in_length, void * out_data, uint32_t max_out_size){
	OpusEncoder * opus = (OpusEncoder *) ctx;
	return opus_encode(opus, (const opus_int16 *)in_data, in_length, (unsigned char *)out_data, max_out_size);
}

audio_codec_ops opus_enc_ops = {
	opus_enc_size,
	opus_enc_init,
	opus_enc_destroy,
	opus_enc_frame_range,
	opus_enc_process
};

typedef struct opus_decoder_s{
	OpusDecoder * opus_dec;
}opus_decoder_t;

int opus_dec_size(const audio_codec_param * param){
	return TRIM_STRUCT_SIZE(opus_decoder_get_size(param->channels));
}

int opus_dec_init(void * ctx, const audio_codec_param * param){
	OpusDecoder * opus = (OpusDecoder *) ctx;
	int ret = opus_decoder_init(opus, param->sample_rate, param->channels);
	return ret;
}

void opus_dec_destroy(void * ctx){
	OpusDecoder * opus = (OpusDecoder *) ctx;
//	opus_decoder_destroy(opus);
	opus_decoder_ctl(opus, OPUS_RESET_STATE);
}

int opus_dec_frame_range(void * ctx, int *min, int *max, int *recommend){
	OpusDecoder * opus = (OpusDecoder *) ctx;
	int sample_rate;
	opus_decoder_ctl(opus, OPUS_GET_SAMPLE_RATE(&sample_rate));
	*min = 1;
	*max = sample_rate;
	*recommend = sample_rate/20;
	return 0;
}

int opus_dec_process(void * ctx, const void * in_data, uint32_t in_length, void * out_data, uint32_t max_out_size){
	OpusDecoder * opus = (OpusDecoder *) ctx;
	int ret = opus_decode(opus, (const unsigned char *)in_data, in_length, (opus_int16 *)out_data, max_out_size, 0);
	return ret;
}

audio_codec_ops opus_dec_ops = {
	opus_dec_size,
	opus_dec_init,
	opus_dec_destroy,
	opus_dec_frame_range,
	opus_dec_process
};


typedef struct speex_encoder_s{
	SpeexBits ebits;
	void * enc_state;
	int enc_frame_size;
}speex_encoder_t;

int speex_enc_size(const audio_codec_param * param){
	return TRIM_STRUCT_SIZE(sizeof(speex_encoder_t));
}

int speex_enc_init(void * ctx, const audio_codec_param * param){
	speex_encoder_t * enc = (speex_encoder_t*) ctx;
	speex_bits_init(&enc->ebits);
	enc->enc_state = speex_encoder_init(&speex_wb_mode);
	speex_encoder_ctl(enc->enc_state, SPEEX_GET_FRAME_SIZE, &enc->enc_frame_size);
	return 0;
}

void speex_enc_destroy(void * ctx){
	speex_encoder_t * enc = (speex_encoder_t *) ctx;
	speex_encoder_destroy(enc->enc_state);
	speex_bits_destroy(&enc->ebits);
}

int speex_enc_frame_range(void * ctx, int *min, int *max, int *recommend){
	speex_encoder_t * enc = (speex_encoder_t *) ctx;
	*min = enc->enc_frame_size;
	*max = enc->enc_frame_size;
	*recommend = enc->enc_frame_size;
	return 0;
}

int speex_enc_process(void * ctx, const void * in_data, uint32_t in_length, void * out_data, uint32_t max_out_size){
	speex_encoder_t * spx = (speex_encoder_t *) ctx;
	if(in_length != spx->enc_frame_size){
		dbge("speex error pcm size %d != %d", in_length, spx->enc_frame_size);
		return -1;
	}
	speex_bits_reset(&spx->ebits);
	speex_encode_int(spx->enc_state, (spx_int16_t *)in_data, &spx->ebits);
	int enc_len = speex_bits_write(&spx->ebits, (char *)out_data, max_out_size );
	return enc_len;
}

audio_codec_ops speex_enc_ops = {
	speex_enc_size,
	speex_enc_init,
	speex_enc_destroy,
	speex_enc_frame_range,
	speex_enc_process
};

typedef struct speex_decoder_s{
	SpeexBits dbits;
	void * dec_state;
	int dec_frame_size;
}speex_decoder_t;

int speex_dec_size(const audio_codec_param * param){
	return TRIM_STRUCT_SIZE(sizeof(speex_decoder_t));
}

int speex_dec_init(void * ctx, const audio_codec_param * param){
	speex_decoder_t * dec = (speex_decoder_t*) ctx;
	speex_bits_init(&dec->dbits);
	dec->dec_state = speex_decoder_init(&speex_wb_mode);
	speex_decoder_ctl(dec->dec_state, SPEEX_GET_FRAME_SIZE, &dec->dec_frame_size);
	return 0;
}

void speex_dec_destroy(void * ctx){
	speex_decoder_t * dec = (speex_decoder_t *) ctx;
	speex_decoder_destroy(dec->dec_state);
	speex_bits_destroy(&dec->dbits);
}

int speex_dec_frame_range(void * ctx, int *min, int *max, int *recommend){
	speex_decoder_t * dec = (speex_decoder_t *) ctx;
	*min = dec->dec_frame_size;
	*max = dec->dec_frame_size;
	*recommend = dec->dec_frame_size;
	return 0;
}

int speex_dec_process(void * ctx, const void * in_data, uint32_t in_length, void * out_data, uint32_t max_out_size){
	speex_decoder_t * spx = (speex_decoder_t *) ctx;

    speex_bits_read_from(&spx->dbits, (char *)in_data, in_length);

    // short int * p = ((short int *)out_data);
    int pcm_len = 0;
	while (speex_bits_remaining(&spx->dbits) >= 5
			&& speex_bits_peek_unsigned(&spx->dbits, 5) != 0xF) {
		if((max_out_size - pcm_len) < spx->dec_frame_size){
			dbge("no enough space for speex decoding\n");
			return -1;
		}

		short int * p = ((short int *)out_data) + pcm_len;
		int ret = speex_decode_int(spx->dec_state, &spx->dbits, p);
		if(ret < 0){
			dbge("speex_decode_int error %d\n", ret);
			return ret;
		}
		pcm_len += spx->dec_frame_size;
		p += spx->dec_frame_size;
	}
	return pcm_len;
}

audio_codec_ops speex_dec_ops = {
	speex_dec_size,
	speex_dec_init,
	speex_dec_destroy,
	speex_dec_frame_range,
	speex_dec_process
};



typedef struct audio_trans_codec{
	void *ctx;
//	int codec;
	const audio_codec_ops * ops;
	int frame_min;
	int frame_max;
	int frame_recommend;
}audio_trans_codec;

static
int audio_trans_codec_size(const audio_codec_ops * ops, const audio_codec_param * param){
	return TRIM_STRUCT_SIZE(sizeof(audio_trans_codec))
			+ TRIM_STRUCT_SIZE(ops->context_size(param));
}

static
int audio_trans_codec_init(audio_trans_codec * c, const audio_codec_ops * ops, const audio_codec_param * param, int dst_frame_size){
	int ret;
	void * ctx = ((uint8_t *)c) + TRIM_STRUCT_SIZE(sizeof(audio_trans_codec));
	ret = ops->init(ctx, param);
	if(ret != 0){
		return ret;
	}
	c->ctx = ctx;

	if(dst_frame_size > 0){
		c->frame_min = dst_frame_size;
		c->frame_max = dst_frame_size;
		c->frame_recommend = dst_frame_size;
	}else{
		ret = ops->frame_range(c->ctx, &c->frame_min, &c->frame_max, &c->frame_recommend);
		if(ret != 0){
			ops->destory(c->ctx);
			c->ctx = NULL;
			return ret;
		}
	}


	c->ops = ops;

	return 0;
}

static
void audio_trans_codec_destory(audio_trans_codec * c){
	if(!c) return;
	if(c->ctx){
		c->ops->destory(c->ctx);
		c->ctx = NULL;
	}
}

#define TRNASCODE_PCM_SIZE (8*1024)
struct audio_transcoder_s{
	audio_codec_param param;

	int16_t pcm_data[TRNASCODE_PCM_SIZE];
	uint32_t pcm_offset;
	uint32_t pcm_len;
	uint32_t timestamp;
	uint32_t next_timestamp;

	void * p;
	audio_trans_codec *src;
	audio_trans_codec *dst;

	int src_codec;
	int dst_codec;
	union{
		opus_encoder_t opus;
		speex_encoder_t speex;
	}enc;

	union{
		opus_decoder_t opus;
		speex_decoder_t speex;
	}dec;


};



void audio_transcoder_close(audio_transcoder_t * obj){
	if(!obj) return;

	if(obj->src){
		audio_trans_codec_destory(obj->src);
		obj->src = NULL;
	}

	if(obj->dst){
		audio_trans_codec_destory(obj->dst);
		obj->dst = NULL;
	}

	if(obj->p){
		free(obj->p);
		obj->p = NULL;
	}
}

static const audio_codec_ops * get_encoder_ops(int codec){
	if(codec == XCODEC_OPUS){
		return &opus_enc_ops;
	}else if(codec == XCODEC_SPEEX){
		return &speex_enc_ops;
	}else{
		return NULL;
	}
}

static const audio_codec_ops * get_decoder_ops(int codec){
	if(codec == XCODEC_OPUS){
		return &opus_dec_ops;
	}else if(codec == XCODEC_SPEEX){
		return &speex_dec_ops;
	}else{
		return NULL;
	}
}

static audio_codec_param default_audio_codec_param = audio_codec_param_build(16000, 1, 16);

int audio_transcoder_get_size(const audio_codec_param * param, int src_codec, int dst_codec, int dst_frame_size){

	int ret = 0;
	const audio_codec_ops * src_ops;
	const audio_codec_ops * dst_ops;

	if(!param) param = &default_audio_codec_param;



	src_ops = get_decoder_ops(src_codec);
	if(!src_ops){
		dbge("unknown src codec %d", src_codec);
		ret = -1;
		return ret;
	}

	dst_ops = get_encoder_ops(dst_codec);
	if(!dst_ops){
		dbge("unknown dst codec %d", dst_codec);
		ret = -1;
		return ret;
	}


	int sz = TRIM_STRUCT_SIZE(audio_trans_codec_size(src_ops, param))
			+ TRIM_STRUCT_SIZE(audio_trans_codec_size(dst_ops, param));

	return TRIM_STRUCT_SIZE(sizeof(struct audio_transcoder_s)) + sz;

}

static
int _audio_transcoder_init(audio_transcoder_t * obj, uint8_t *p, const audio_codec_param * param, int src_codec, int dst_codec, int dst_frame_size){
	int ret = 0;

	if(!param) param = &default_audio_codec_param;
	memset(obj, 0, sizeof(struct audio_transcoder_s));
	do{
		const audio_codec_ops * src_ops;
		const audio_codec_ops * dst_ops;

		src_ops = get_decoder_ops(src_codec);
		if(!src_ops){
			dbge("unknown src codec %d", src_codec);
			ret = -1;
			break;
		}

		dst_ops = get_encoder_ops(dst_codec);
		if(!dst_ops){
			dbge("unknown dst codec %d", dst_codec);
			ret = -1;
			break;
		}

//		uint8_t *p = NULL;
		if(!p){
			int sz = TRIM_STRUCT_SIZE(audio_trans_codec_size(src_ops, param))
					+ TRIM_STRUCT_SIZE(audio_trans_codec_size(dst_ops, param));
			p = (uint8_t*) malloc(sz) ;
			memset(p, 0, sz);
			obj->p = p;
		}
		obj->param = *param;
		obj->src = (audio_trans_codec *)p;
		ret = audio_trans_codec_init(obj->src, src_ops, param, 0);
		if(ret < 0){
			dbge("init src codec error %d", ret);
			break;
		}
		p += TRIM_STRUCT_SIZE(audio_trans_codec_size(src_ops, param));

		obj->dst = (audio_trans_codec *)p;
		ret = audio_trans_codec_init(obj->dst, dst_ops, param, dst_frame_size);
		if(ret < 0){
			dbge("init dst codec error %d", ret);
			break;
		}
		p += TRIM_STRUCT_SIZE(audio_trans_codec_size(dst_ops, param));


		ret = 0;

	}while(0);

	if(ret != 0){
		audio_transcoder_close(obj);
	}


	return ret;
}

audio_transcoder_t * audio_transcoder_create(const audio_codec_param * param, int src_codec, int dst_codec, int dst_frame_size){
	int sz = audio_transcoder_get_size(param, src_codec, dst_codec, dst_frame_size);
	if(sz <= 0) return NULL;
	uint8_t *p = (uint8_t*) malloc(sz);
	memset(p, 0, sz);
	audio_transcoder_t * obj = (audio_transcoder_t *) p;
	int ret = _audio_transcoder_init(obj
			, NULL //p+TRIM_STRUCT_SIZE(sizeof(struct audio_transcoder_s))
			, param, src_codec, dst_codec, dst_frame_size);
	if(ret != 0){
		free(obj);
		return NULL;
	}
	return obj;
}

void audio_transcoder_free(audio_transcoder_t * obj){
	if(obj){
		audio_transcoder_close(obj);
		free(obj);
	}
}

int audio_transcoder_init(audio_transcoder_t * obj, const audio_codec_param * param, int src_codec, int dst_codec, int dst_frame_size){
	return _audio_transcoder_init(obj, NULL, param, src_codec, dst_codec, dst_frame_size );
}


//static void * internal_ctx1 = 0;
int audio_transcoder_input(audio_transcoder_t * obj
		, uint32_t in_timestamp, const uint8_t * in_data, uint32_t in_length){
//	dbgi("audio_transcoder_input: obj=%p, len=%d\n", obj, in_length);
	int ret;
	if(in_timestamp > 0 && in_timestamp != obj->next_timestamp){
		obj->pcm_offset = 0;
		obj->pcm_len = 0;
		obj->timestamp = in_timestamp;
	}
	int16_t * pcm = obj->pcm_data + obj->pcm_len;
	uint32_t pcm_size = TRNASCODE_PCM_SIZE - obj->pcm_len;
	ret = obj->src->ops->process(obj->src->ctx, in_data, in_length, pcm, pcm_size);
	if(ret < 0){
		dbge("decode src error %d\n", ret);
		return ret;
	}

	obj->pcm_len += ret;
	obj->next_timestamp = in_timestamp + ret/obj->param.channels;

//	xdebug_write_to_file(&internal_ctx1, "/tmp/internal_trans1.pcm", pcm, ret*2);
	return 0;
}

//static void * internal_ctx2 = 0;
int audio_transcoder_next(audio_transcoder_t * obj
		, uint32_t *out_timestamp, uint8_t * out_data, uint32_t max_out_size){
//	dbgi("audio_transcoder_next: => obj=%p, max_out_size=%d\n", obj, max_out_size);
	int ret;
	int remains = obj->pcm_len-obj->pcm_offset;
	// uint8_t * enc_data = out_data;
	// uint32_t enc_data_size = max_out_size;
	spx_int16_t * pcm = obj->pcm_data+obj->pcm_offset;
	uint32_t trans_len = 0;
	if(remains < obj->dst->frame_min ){
		return 0;
	}


	uint32_t enc_frame_size = remains <= obj->dst->frame_max ? remains : obj->dst->frame_max;
//		dbgi("ddd remains=%d, enc_frame_size=%d, dst->frame_max=%d\n", remains, enc_frame_size, obj->dst->frame_max);
//	xdebug_write_to_file(&internal_ctx2, "/tmp/internal_trans2.pcm", pcm, enc_frame_size*2);
	ret = obj->dst->ops->process(obj->dst->ctx, pcm, enc_frame_size, out_data+trans_len, max_out_size-trans_len);
	if(ret < 0){
		dbge("encode dst error %d", ret);
		return ret;
	}
	trans_len = ret;
	if(out_timestamp){
		*out_timestamp = obj->timestamp;
	}

	remains -= enc_frame_size;
	obj->pcm_offset += enc_frame_size;
	obj->timestamp += enc_frame_size/obj->param.channels;

	if (remains < obj->dst->frame_min) {
		if(remains > 0){
			spx_int16_t * src = obj->pcm_data+obj->pcm_offset;
			memmove(obj->pcm_data, src, remains*sizeof(int16_t));
		}
		obj->pcm_len = remains;
		obj->pcm_offset = 0;
	}
//	dbgi("audio_transcoder_next: <= obj=%p, trans_len=%d\n", obj, trans_len);
	return trans_len;
}

int audio_transcoder_init0(audio_transcoder_t * obj, int src_codec, int dst_codec){
	int ret = 0;
	memset(obj, 0, sizeof(audio_transcoder_t));
	obj->src_codec = src_codec;
	obj->dst_codec = dst_codec;
	do{
		if(obj->src_codec == XCODEC_OPUS){
			opus_decoder_t * dec = &obj->dec.opus;
			int error = 0;
			dec->opus_dec = opus_decoder_create (16000, 1, &error);
			if(error != OPUS_OK){
				dbge("opus_decoder_create error=%d ", error);
				ret = -1;
				break;
			}
		}else if(obj->src_codec == XCODEC_SPEEX){
			speex_decoder_t * dec = &obj->dec.speex;
			speex_bits_init(&dec->dbits);
			dec->dec_state = speex_decoder_init(&speex_wb_mode);
			speex_decoder_ctl(dec->dec_state, SPEEX_GET_FRAME_SIZE, &dec->dec_frame_size);
		}else{
			dbge("unknown src codec %d", obj->src_codec);
			ret = -1;
			break;
		}

		if(obj->dst_codec == XCODEC_OPUS){
			opus_encoder_t * enc = &obj->enc.opus;
			int error = 0;
			enc->opus_enc = opus_encoder_create(16000, 1, OPUS_APPLICATION_AUDIO, &error);
			if(error != OPUS_OK){
				dbge("opus_encoder_create error=%d ", error);
				ret = -1;
				break;
			}
		}else if(obj->dst_codec == XCODEC_SPEEX){
			speex_encoder_t * enc = &obj->enc.speex;
			speex_bits_init(&enc->ebits);
			enc->enc_state = speex_encoder_init(&speex_wb_mode);
			speex_encoder_ctl(enc->enc_state, SPEEX_GET_FRAME_SIZE, &enc->enc_frame_size);
		}else{
			dbge("unknown dst codec %d", obj->dst_codec);
			ret = -1;
			break;
		}

		ret = 0;

	}while(0);

	if(ret != 0){
		audio_transcoder_close(obj);
	}

	return ret;

}

int test_codec(const char * in_pcm_file
		, const char * out_pcm_file
		, audio_codec_param * param
		, int src_codec
		, int dst_codec){

	audio_trans_codec * enc = (audio_trans_codec *) malloc(audio_trans_codec_size(get_encoder_ops(src_codec), param));
	audio_trans_codec * dec = (audio_trans_codec *) malloc(audio_trans_codec_size(get_decoder_ops(dst_codec), param));
	int ret;

	do{
		ret = audio_trans_codec_init(enc, get_encoder_ops(src_codec), param, 0);
		if(ret < 0){
			dbge("test: init encoder error %d\n", ret);
			break;
		}

		ret = audio_trans_codec_init(dec, get_decoder_ops(dst_codec), param, 0);
		if(ret < 0){
			dbge("test: init decoder error %d\n", ret);
			break;
		}

		int pcm_samples = enc->frame_recommend;
		int pcm_frame_size = pcm_samples*sizeof(int16_t);
		int enc_frame_size = pcm_frame_size*2;
		int16_t * pcm = (int16_t *) malloc(pcm_frame_size);
		uint8_t * enc_data = (uint8_t *) malloc(enc_frame_size);
		FILE * fp = fopen(in_pcm_file, "rb");
		void * fp_out = 0;
		while(1){
			int bytes_read = fread(pcm, 1, pcm_frame_size, fp);
			if(bytes_read != pcm_frame_size){
				dbgi("test: reach input file end\n");
				break;
			}

			int enc_len = enc->ops->process(enc->ctx, pcm, pcm_samples, enc_data, enc_frame_size);
			if(enc_len <= 0){
				dbge("test: encode error %d\n", enc_len);
				break;
			}

			memset(pcm , 0, pcm_frame_size);

			int dec_len = dec->ops->process(dec->ctx, enc_data, enc_len, pcm, pcm_samples);
			if (dec_len <= 0) {
				dbge("test: decode error %d\n", dec_len);
				break;
			}
			xdebug_write_to_file(&fp_out, out_pcm_file, pcm, dec_len*sizeof(int16_t));
		}
	}while(0);

	dbgi("test done\n");
	audio_trans_codec_destory(enc);
	audio_trans_codec_destory(dec);
	free(enc);
	free(dec);
	return 0;
}


int test_transcode(const char * in_pcm_file
		, const char * out_pcm_file
		, audio_codec_param * param
		, int src_codec
		, int dst_codec){
	audio_transcoder_t obj_stor;
	audio_transcoder_t * obj = 0;
	audio_trans_codec * enc = (audio_trans_codec *) malloc(audio_trans_codec_size(get_encoder_ops(src_codec), param));
	audio_trans_codec * dec = (audio_trans_codec *) malloc(audio_trans_codec_size(get_decoder_ops(dst_codec), param));
	int ret;

	do{
		ret = audio_transcoder_init(&obj_stor, param, src_codec, dst_codec, 0);
		if (ret < 0) {
			dbge("test: audio_transcoder_init error %d\n", ret);
			break;
		}
		obj = &obj_stor;

		ret = audio_trans_codec_init(enc, get_encoder_ops(src_codec), param, 0);
		if(ret < 0){
			dbge("test: init encoder error %d\n", ret);
			break;
		}

		ret = audio_trans_codec_init(dec, get_decoder_ops(dst_codec), param, 0);
		if(ret < 0){
			dbge("test: init decoder error %d\n", ret);
			break;
		}

		int in_pcm_samples = enc->frame_recommend;
		int in_pcm_frame_size = in_pcm_samples*sizeof(int16_t);
		int enc_frame_size = in_pcm_frame_size*2;
		int out_pcm_samples = dec->frame_recommend;
		// int out_pcm_frame_size = out_pcm_samples*sizeof(int16_t);
		int pcm_samples = in_pcm_samples > out_pcm_samples ? in_pcm_samples : out_pcm_samples;
		int pcm_size = pcm_samples * sizeof(int16_t) ;
		uint32_t timestamp = 0;

		uint8_t * enc_data = (uint8_t *) malloc(enc_frame_size);
		uint8_t * transcode_data = (uint8_t *) malloc(enc_frame_size);
		int16_t * pcm = (int16_t *) malloc(pcm_size);

		FILE * fp = fopen(in_pcm_file, "rb");
		void * fp_out = 0;
		while(1){
			int bytes_read = fread(pcm, 1, in_pcm_frame_size, fp);
			if(bytes_read != in_pcm_frame_size){
				dbgi("test: reach input file end\n");
				break;
			}

			dbgi("test: in_pcm_samples=%d\n", in_pcm_samples);
			int enc_len = enc->ops->process(enc->ctx, pcm, in_pcm_samples, enc_data, enc_frame_size);
			if(enc_len <= 0){
				dbge("test: encode error %d\n", enc_len);
				break;
			}
			dbgi("test: encode length=%d\n", enc_len);

			ret = audio_transcoder_input(obj, timestamp, enc_data, enc_len);
			if(ret < 0){
				dbge("test: audio_transcoder_input error %d\n", ret);
				break;
			}

			int trans_len;
			while((trans_len = audio_transcoder_next(obj, NULL,  transcode_data, enc_frame_size)) > 0){
				memset(pcm , 0, pcm_size);

				int dec_len = dec->ops->process(dec->ctx, transcode_data, trans_len, pcm, pcm_samples);
//				int dec_len = speex_dec_process(dec->ctx, transcode_data, trans_len, pcm, pcm_samples);
				if (dec_len <= 0) {
					dbge("test: decode error %d\n", dec_len);
					break;
				}
				dbgi("test: out length=%d\n", dec_len);
				xdebug_write_to_file(&fp_out, out_pcm_file, pcm, dec_len*sizeof(int16_t));
			}

			timestamp += in_pcm_samples;
		}
	}while(0);

	dbgi("test_transcode done\n");
	audio_trans_codec_destory(enc);
	audio_trans_codec_destory(dec);
	free(enc);
	free(dec);
	return 0;
}


int audio_transcoder_test(){
	const char * in_pcm_file = "/tmp/tmp/count_test_16kHz_s16le_mono.pcm";
	audio_codec_param audio_param = audio_codec_param_build(16000, 1, 16);

	int ret = 0;
//	ret = test_codec(in_pcm_file, "/tmp/opus.pcm", &audio_param, XCODEC_OPUS, XCODEC_OPUS);
//	ret = test_codec(in_pcm_file, "/tmp/speex.pcm", &audio_param, XCODEC_SPEEX, XCODEC_SPEEX);
	ret = test_transcode(in_pcm_file, "/tmp/transcode_opus2speex.pcm", &audio_param, XCODEC_OPUS, XCODEC_SPEEX);
	ret = test_transcode(in_pcm_file, "/tmp/transcode_speex2opus.pcm", &audio_param, XCODEC_SPEEX, XCODEC_OPUS);
	exit(0);
	return ret;
}

