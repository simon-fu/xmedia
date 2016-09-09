
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>


#include "xcutil.h"
#include "xrtp_h264.h"
#define LOG_PLAY(...)
#define dbgd(...) //do{printf(__VA_ARGS__); printf("\n");}while(0)
#define dbgi(...) do{printf(__VA_ARGS__); printf("\n");}while(0)





int xrtp_to_nalu_init(xrtp_to_nalu_t *ctx, uint8_t *nalu_buf,
		uint32_t nalu_buf_size, uint32_t nalu_buf_offset) {
	dbgd("xrtp_to_nalu_init: nalu_buf=%p, nalu_buf_size=%d, nalu_buf_offset=%d"
			, nalu_buf, nalu_buf_size, nalu_buf_offset);

	memset(ctx, 0, sizeof(*ctx));
	ctx->nalu_buf = nalu_buf;
	ctx->nalu_buf_size = nalu_buf_size;

	if(!ctx->nalu_buf){
		if(ctx->nalu_buf_size == 0){
			ctx->nalu_buf_size = 64*1024;
		}
		dbgd("malloc nalu_buf_size %d", ctx->nalu_buf_size);
		ctx->internal_nalu_buf = (uint8_t *) malloc(ctx->nalu_buf_size);
		ctx->nalu_buf = ctx->internal_nalu_buf;
	}
	ctx->nalu_buf += nalu_buf_offset;


	return 0;
}

void xrtp_to_nalu_close(xrtp_to_nalu_t * ctx){
	if(ctx->internal_nalu_buf){
		free(ctx->internal_nalu_buf);
		ctx->internal_nalu_buf = NULL;
	}
}


int xrtp_to_nalu_next(xrtp_to_nalu_t *ctx, const uint8_t *rtp, uint32_t rtp_len) {

	if(rtp_len <=XRTP_HEADER_LEN)
	{
		dbgd("RTP packet length too short,ignored");
		ctx->nalu_len = 0;
		return -1;
	}

	int& m_naltype = ctx->m_naltype;
	const uint8_t *pRtpHeader = rtp;
	unsigned char  media_type = pRtpHeader[1]&0x7F;
	bool is_mark = pRtpHeader[1]&0x80;
	unsigned int ts = *( (uint32_t*)(pRtpHeader + 4) );
	ts = ntohl(ts);
	unsigned short rtp_seq =  *( (uint16_t*)(pRtpHeader + 2) );
	rtp_seq = ntohs(rtp_seq);
	
	unsigned char x =  (pRtpHeader[0]>>4) & 0x1;
	unsigned char cc = (pRtpHeader[0]>>0) & 0xF;
	int header_len = XRTP_HEADER_LEN;
	if(cc){
		header_len += cc * 4;
		if(rtp_len <=header_len)
		{
			dbgd("RTP packet (cc) length too short,ignored");
			ctx->nalu_len = 0;
			return -1;
		}
	}
	
	if(x){
		int min = header_len + 4;
		if(min <= rtp_len){
			int ext_len = be_get_u16(pRtpHeader+header_len+2);
			header_len += 4+ext_len*4;
		}else{
			header_len = -1;
			dbgd("RTP packet (ext) length too short,ignored");
			ctx->nalu_len = 0;
			return -1;
		}
	}




	unsigned char nal_type = 0;
	unsigned char   nal_data_offset = header_len;
	int ret = rtp_len;
	unsigned frag_len;


	dbgd("header_len=%d, new rtp seq:%d, old rtp seq:%d", header_len, rtp_seq, ctx->last_seq);

	if(ctx->last_nalu_done){
		ctx->last_nalu_done = 0;
		ctx->nalu_len = 0;
	}

	uint32_t next_seq = (ctx->last_seq+1)%65536;
	if( (rtp_seq != next_seq))
	{
		ctx->m_hasIFrame = 0; // avoid mosaic
		ctx->nalu_len = 0;
		dbgi("rtp seq disorder: last=%d, curr=%d ~~~", ctx->last_seq, (unsigned) rtp_seq);
	}

	if(rtp_seq < 200 && ctx->last_seq > (65536-200)){
		dbgi("rtp seq round back: last=%d, curr=%d ~~~", ctx->last_seq, (unsigned) rtp_seq);
	}else if(rtp_seq < ctx->last_seq){
		dbgi("rtp seq old");
		return 0;
	}


	ctx->last_seq = rtp_seq;
	ctx->timestamp = ts;


	nal_type = pRtpHeader[nal_data_offset]&0x1f;
	dbgd("rtp2nalu: nal_type %d", nal_type);
	if (nal_type > 0 && nal_type < 24)
	{

		ctx->m_FrameFlag =0;
////		ctx->nalu_len = 0;
//        ctx->nalu_buf[ctx->nalu_len++] = 0x00;
//        ctx->nalu_buf[ctx->nalu_len++] = 0x00;
//        ctx->nalu_buf[ctx->nalu_len++] = 0x00;
//        ctx->nalu_buf[ctx->nalu_len++] = 0x01;
        frag_len = rtp_len - nal_data_offset;
        memcpy (ctx->nalu_buf+ctx->nalu_len,
        		pRtpHeader + nal_data_offset,
				frag_len);
		m_naltype = ctx->nalu_buf[ctx->nalu_len]&0x1f;

		ctx->nalu_len  += frag_len;
//		if(is_mark)
//		{
//			if(m_naltype == 7 || m_naltype == 8 || ctx->m_hasIFrame){
//				ret = process_nalu( ts,ctx->nalu_buf,ctx->nalu_len);
//			}
//			ctx->nalu_len = 0;
//		}
		ctx->m_rtpseq = rtp_seq;
		dbgd("rtp2nalu: 1 -> 1, frag_len=%d, nalu_len=%d", frag_len, ctx->nalu_len);
		ctx->last_nalu_done = 1;
        return ctx->nalu_len;
	}


	if(nal_type == 28) // FU-A (fragmented nal)
	{
            /*
            0                   1                   2                   3
            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            | FU indicator  |   FU header   |                               |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
            |                                                               |
            |                         FU payload                            |
            |                                                               |
            |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |                               :...OPTIONAL RTP padding        |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            */
       // these are the same as above, we just redo them here for clarity...
         unsigned char fu_indicator = *(pRtpHeader +nal_data_offset);
         // skip the fu_indicator
         nal_data_offset ++;
		 unsigned char fu_header  = *(pRtpHeader +nal_data_offset);   // read the fu_header.
		 unsigned char start_bit  = fu_header >> 7;
		 unsigned char end_bit   = (fu_header >> 6) & 1;
		 unsigned char reconstructed_nal;
		 // reconstruct this packet's true nal; only the data follows..
		 reconstructed_nal = fu_indicator & 0xe0;  // the original nal forbidden bit and NRI are stored in this packet's nal;
         reconstructed_nal |= fu_header&0x1f;
		 // skip the fu_header...
		 nal_data_offset ++;
		 if(start_bit)
		 {
////			 ctx->nalu_len = 0;
//			 ctx->nalu_buf[ctx->nalu_len++] = 0x00;
//			 ctx->nalu_buf[ctx->nalu_len++] = 0x00;
//			 ctx->nalu_buf[ctx->nalu_len++] = 0x00;
//			 ctx->nalu_buf[ctx->nalu_len++] = 0x01;
			 ctx->nalu_buf[ctx->nalu_len++] = reconstructed_nal;
			m_naltype = reconstructed_nal&0x1f;
			if(!ctx->m_hasIFrame)
			{
				ctx->m_hasIFrame = (m_naltype == 5);
			}
	   		ctx->m_FrameFlag=1;
	   		ctx->m_rtpseq = rtp_seq ;
	   		frag_len = rtp_len -nal_data_offset;
			memcpy (ctx->nalu_buf+ctx->nalu_len,pRtpHeader +nal_data_offset, frag_len);
			ctx->nalu_len += (frag_len);
			dbgd("rtp2nalu: fu -> start_bit=%d, frag_len=%d, nalu_len=%d", start_bit, frag_len, ctx->nalu_len);
		 }
		 else
		 {
		 	if(ctx->m_FrameFlag == 0)
	  		{
		 		dbgd("rtp2nalu: fu -> end_bit=%d, m_FrameFlag=0~~~", end_bit);
		 		ctx->nalu_len=0;
				return 0;
	  		}
         	else
	  		{
	  			if((ctx->m_rtpseq+1)%65536 != rtp_seq)
				{
	  				dbgd("rtp2nalu: fu -> end_bit=%d, disorder", end_bit);
	  				ctx->nalu_len=0;
	  				ctx->m_FrameFlag =0;
					return 0;
				}
	  		}
		 	ctx->m_rtpseq++;
		 	frag_len = rtp_len -nal_data_offset;
            memcpy (ctx->nalu_buf+ctx->nalu_len,pRtpHeader +nal_data_offset, frag_len);
            ctx->nalu_len += (frag_len);
            dbgd("rtp2nalu: fu -> end_bit=%d, frag_len=%d, nalu_len=%d", end_bit, frag_len, ctx->nalu_len);
		}

		//if(end_bit&&ctx->m_FrameFlag ==1 && is_mark)
		 if(end_bit&&ctx->m_FrameFlag ==1 )
		{
//			if(m_naltype == 7 || m_naltype == 8 || ctx->m_hasIFrame){
//				ret = process_nalu(ts, ctx->nalu_buf, ctx->nalu_len);
//			}
//			ctx->nalu_len = 0;

			ret = 0;
			ctx->last_nalu_done = 1;
		}

		if(end_bit)
		{
			ctx->m_FrameFlag =0;
		}
		return ctx->last_nalu_done ? ctx->nalu_len : 0;
	}

	dbgi("rtp2nalu: unknown nal_type %d", nal_type);
	return 0;
}

#define XRTP_MAX_RTPSIZE 512
#define XRTP_MAX_PAYLOAD_SIZE (XRTP_MAX_RTPSIZE-XRTP_HEADER_LEN)

static void
build_rtp_head(xnalu_to_rtp_t * ctx, uint8_t *h, uint32_t mask){
	h[0] = 0x80;
	h[1] = (uint8_t)((ctx->payload_type & 0x7f) | ((mask & 0x01) << 7));
	be_set_u16(ctx->seq, h+2);
	be_set_u32(ctx->timestamp, h+4);
	be_set_u32(ctx->ssrc, h+8);

	ctx->seq = (ctx->seq + 1) & 0xffff;
}

int xnalu_to_rtp_init(xnalu_to_rtp_t * ctx, uint32_t ssrc, uint32_t payload_type,
		uint32_t max_rtp_size)
{

	memset(ctx, 0, sizeof(*ctx));
	ctx->ssrc = ssrc;
	ctx->payload_type = payload_type;
	ctx->max_rtp_size = max_rtp_size > 0 ? max_rtp_size : XRTP_MAX_RTPSIZE;
	ctx->seq = 0;

	return 0;
}

int xnalu_to_rtp_first(xnalu_to_rtp_t * ctx, uint32_t timestamp, const uint8_t *nalu,
		uint32_t nalu_len, int mask)
{
	ctx->timestamp = timestamp;
	ctx->pos = nalu;
	ctx->nalu_len = nalu_len;
	ctx->nalu_remains = nalu_len;
	ctx->mask = mask;

	// fix kurento rtpendpoint
	int nalu_type = nalu[0] & 0x1F;
	if(nalu_type == 7 || nalu_type == 8 || nalu_type == 9){
		ctx->mask = 1;
	}


	if (nalu_len <= (ctx->max_rtp_size - XRTP_HEADER_LEN)) {
		ctx->nalu_frag_size = ctx->max_rtp_size - XRTP_HEADER_LEN;
		ctx->is_fu = 0;
	}else{
		ctx->nalu_frag_size = ctx->max_rtp_size - XRTP_HEADER_LEN - 2;
		ctx->is_fu = 1;

	    uint8_t type = nalu[0] & 0x1F;
	    uint8_t nri = nalu[0] & 0x60;
        ctx->fu_head[0] = 28;        /* FU Indicator; Type = 28 ---> FU-A */
        ctx->fu_head[0] |= nri;
        ctx->fu_head[1] = type;
        ctx->fu_head[1] |= 1 << 7; // set start bit

        dbgd("n2r: first, fu[0]=0x%02X, type=%d", ctx->fu_head[0], ctx->fu_head[0]&0x1F);
        ctx->pos  += 1;
        ctx->nalu_remains -= 1;

        ctx->fu_flag_offset = 1;
        ctx->fu_head_len = 2;
	}

	return 0;
}


int xnalu_to_rtp_next(xnalu_to_rtp_t * ctx, uint8_t *rtp)
{
	if(ctx->nalu_remains == 0) return 0;

	int rtp_len;
	if(!ctx->is_fu){ // single RTP
		dbgd("n2r: single");
		build_rtp_head(ctx, rtp, ctx->mask);
		memcpy(rtp+XRTP_HEADER_LEN, ctx->pos, ctx->nalu_len);
		rtp_len = XRTP_HEADER_LEN + ctx->nalu_len;
		ctx->nalu_remains = 0;
		return rtp_len;
	}

	uint32_t mask;
	if(ctx->nalu_remains > ctx->nalu_frag_size){
		dbgd("n2r: fu, fu[0]=0x%02X, type=%d", ctx->fu_head[0], ctx->fu_head[0]&0x1F);
		mask = 0;
		memcpy(rtp+XRTP_HEADER_LEN, ctx->fu_head, ctx->fu_head_len);
        memcpy(&rtp[XRTP_HEADER_LEN+ctx->fu_head_len], ctx->pos, ctx->nalu_frag_size);
        build_rtp_head(ctx, rtp, mask);
        rtp_len = XRTP_HEADER_LEN + ctx->fu_head_len + ctx->nalu_frag_size;

        ctx->pos  += ctx->nalu_frag_size;
        ctx->nalu_remains -= ctx->nalu_frag_size;
        ctx->fu_head[ctx->fu_flag_offset] &= ~(1 << 7); // clear start bit

	}else{
		dbgd("n2r: fu end, fu[0]=0x%02X, type=%d", ctx->fu_head[0], ctx->fu_head[0]&0x1F);
		// mask = 1;
		mask = ctx->mask;
		ctx->fu_head[ctx->fu_flag_offset] |= 1 << 6;; // set end bit
		memcpy(rtp+XRTP_HEADER_LEN, ctx->fu_head, ctx->fu_head_len);
		memcpy(&rtp[XRTP_HEADER_LEN+ctx->fu_head_len], ctx->pos, ctx->nalu_remains);
		build_rtp_head(ctx, rtp, mask);
		rtp_len = XRTP_HEADER_LEN + ctx->fu_head_len + ctx->nalu_remains;
		ctx->nalu_remains = 0;
	}
	dbgd("n2r: return %d", rtp_len);
	return rtp_len;

}

int test_rtp_nalu()
{
	const char * fname = "/tmp/rec.h264";
	FILE * fp = 0;
	uint8_t * nalu;

	nalu = (uint8_t *) malloc(64*1024);
	fp = fopen(fname, "rb");

	int bytesread = fread(nalu,1,64*1024, fp);
	dbgd("read bytes %d", bytesread);

	int start = 0;
	int end = 0;
	int nalu_len;
	for(int i = 0; i < (bytesread - 4); ){
		if((nalu[i] == 0 && nalu[i+1] == 0 && nalu[i+2] == 1)){
			if(start == 0 ) {
				start = i+3;
				dbgd("an3: start=%d", start);
			}else if(end == 0 && start > 0) {
				end = i;
				dbgd("an3 end=%d", end);
			}
			i += 3;
		}else if (nalu[i] == 0 && nalu[i+1] == 0 && nalu[i+2] == 0 && nalu[i+3] == 1){
			if (start == 0) {
				start = i + 4;
				dbgd("an4: start=%d", start);
			}else if (end == 0 && start > 0) {
				end = i;
				dbgd("an4 end=%d", end);
			}
			i += 4;
		}else{
			i++;
		}


		if(end > 0 && start > 0){
			nalu_len = end-start;
			int nal_type = nalu[start] & 0x1F;
			if(nalu_len > 2048 && nal_type == 5){
				break;
			}
			nalu_len = 0;
			end = start = 0;
		}

	}

	if(nalu_len == 0){
		dbgd("NOT found nalu");
	}

	int nal_type = nalu[start] & 0x1F;
	dbgd("got nalu: len=%d, type=%d", nalu_len, nal_type);

	xnalu_to_rtp_t n2r;
	xrtp_to_nalu_t r2n;
	uint8_t * out_nalu = (uint8_t *) malloc(64*1024);
	uint8_t rtp[1600];

	xnalu_to_rtp_init(&n2r, 1111, 96, 512);
	xrtp_to_nalu_init(&r2n, out_nalu, 64*1024, 0);

	xnalu_to_rtp_first(&n2r, 1, &nalu[start], nalu_len, 1);
	uint32_t len;
	while((len=xnalu_to_rtp_next(&n2r, rtp)) > 0){
		int ret = xrtp_to_nalu_next(&r2n, rtp, len);
		if(ret < 0){
			dbgd("xrtp_to_nalu_next error %d", ret);
			break;
		}
		if(ret == 0){
			dbgd("out nalu done");
			if((r2n.nalu_len-4) == nalu_len
					&& memcmp(r2n.nalu_buf+4, &nalu[start], nalu_len) == 0){
				dbgd("nalu equ");
			}else{
				dbgd("nalu NOT equ");
			}
			break;
		}
	}

	exit(0);
	return 0;
}



int xtimestamp64_init(xtimestamp64 * obj){
	memset(obj, 0, sizeof(xtimestamp64));
	return 0;
}

int64_t xtimestamp64_unwrap(xtimestamp64 * obj, uint32_t ts){
  if (ts < obj->last_ts_) {
    if (obj->last_ts_ > 0xf0000000 && ts < 0x0fffffff) {
      ++obj->num_wrap_;
    }
  }
  obj->last_ts_ = ts;
  int64_t unwrapped_ts = ts + (obj->num_wrap_ << 32);
  return unwrapped_ts;
}


static
uint32_t rebase_timestamp(xtimestamp64 * tswrapper, int64_t &src_first_timestamp, unsigned char * rtp){
	uint32_t timestamp = be_get_u32(rtp+4);
	int64_t timestamp64 = xtimestamp64_unwrap(tswrapper, timestamp);
	if(src_first_timestamp < 0){
		src_first_timestamp = timestamp64;
		dbgi("first timestamp %lld", src_first_timestamp);
	}
	timestamp = (uint32_t)(timestamp64 - src_first_timestamp);
	be_set_u32(timestamp, rtp+4);
	return timestamp;
}

void xrtp_transformer_init(xrtp_transformer * obj, uint32_t src_samplerate, uint32_t dst_samplerate, uint32_t dst_payloadtype, uint32_t dst_ssrc){
	memset(obj, 0, sizeof(xrtp_transformer));
	obj->src_samplerate = src_samplerate;
	obj->dst_samplerate = dst_samplerate;
	obj->dst_payloadtype = dst_payloadtype;
	obj->dst_ssrc = dst_ssrc;
	xtimestamp64_init(&obj->tswrapper);
	obj->src_first_timestamp = -1;
}

void xrtp_transformer_process(xrtp_transformer * obj, unsigned char * rtp){
	if(obj->dst_payloadtype > 0){
		rtp[1] &= 0x80;
		rtp[1] |= (uint8_t)( (obj->dst_payloadtype & 0x7f) );
	}

	// rebase timestamp
	uint32_t timestamp = rebase_timestamp(&obj->tswrapper, obj->src_first_timestamp, rtp);

	// transform timestamp
	if(obj->dst_samplerate > 0 && obj->src_samplerate > 0){
		
		int64_t t64 = timestamp;
		// dbgi("timestame1 = %u", timestamp);
		timestamp = (t64 * obj->dst_samplerate/obj->src_samplerate);	
		be_set_u32(timestamp, rtp+4);		
	}

	if(obj->dst_ssrc > 0){
		be_set_u32(obj->dst_ssrc, rtp+8);
	}

	be_set_u16(obj->last_seq, rtp+2);
	obj->last_seq++;

}





void xrtp_h264_repacker_init(xrtp_h264_repacker * obj
		, uint8_t *nalu_buf, uint32_t nalu_buf_size, uint32_t nalu_buf_offset
		, uint32_t dst_ssrc, uint32_t dst_payloadtype, uint32_t max_rtp_size
		, uint32_t src_samplerate, uint32_t dst_samplerate){

	memset(obj, 0, sizeof(xrtp_h264_repacker));
	obj->src_samplerate = src_samplerate;
	obj->dst_samplerate = dst_samplerate;
	obj->nalu_buf = nalu_buf;
	
	xtimestamp64_init(&obj->tswrapper);
	obj->src_first_timestamp = -1;

	xrtp_to_nalu_init(&obj->rtp2nalu, nalu_buf, nalu_buf_size, nalu_buf_offset);
	xnalu_to_rtp_init(&obj->nalu2rtp, dst_ssrc, dst_payloadtype, max_rtp_size);
}

static
const char * xnalu_type_string(int nalu_type){
	if(nalu_type == 5) return "(I)";
	if(nalu_type == 7) return "(SPS)";
	if(nalu_type == 8) return "(PPS)";
	return "";
}

int xrtp_h264_repacker_input(xrtp_h264_repacker * obj, unsigned char * rtp, int rtp_len){
	int ret = 0;

	// dump_rtp("video rtp1", data, data_len);
	obj->last_nalu_len = 0;
	int mask = (rtp[1] >> 7) & 0x1;

	ret = xrtp_to_nalu_next(&obj->rtp2nalu, rtp, rtp_len);
	if(ret <= 0) {
		return ret;
	}
	int nalu_len = ret;
	

	int nal_type = obj->nalu_buf[0] & 0x1F;
	if(nal_type == 5 || nal_type == 7  || nal_type == 8 ){
		dbgi("nalu: type=%d%s, len=%d", nal_type, xnalu_type_string(nal_type), nalu_len);	
	}
	
	if(nal_type == 9) {
		return 0;
	}

	// if(nal_type == 7) {// sps
	// 	if(ch->got_sps) return;
	// 	ch->got_sps = 1;
	// }else if(nal_type == 8){ // pps
	// 	if(ch->got_pps) return;
	// 	ch->got_pps = 1;
	// }else if(nal_type == 5){
	// 	ch->got_keyframe = 1;
	// 	dbgi("got keyframe");
	// }else if(nal_type == 9){
	// 	return;
	// }else{
	// 	if(!ch->got_keyframe){
	// 		return;
	// 	}
	// }

	// rebase timestamp
	uint32_t timestamp = rebase_timestamp(&obj->tswrapper, obj->src_first_timestamp, rtp);
	// dbgi("video nalu timestamp %u, len=%d", timestamp, nalu_len);
	
	ret = xnalu_to_rtp_first(&obj->nalu2rtp, timestamp, obj->nalu_buf, nalu_len, mask); 
	if(ret != 0){
		return 0;
	}
	obj->last_nalu_len = nalu_len;
	return obj->last_nalu_len;
}

int xrtp_h264_repacker_next(xrtp_h264_repacker * obj, unsigned char * rtp){
	if(obj->last_nalu_len ==0) return 0;
	return xnalu_to_rtp_next(&obj->nalu2rtp, rtp);
}




#if 0

void SendH264VideoWith3984(struct rtp *video_rtp_session,unsigned char *buffer,int data_len,u_int64_t pts,int mark/*,rc4_encryption *p_encryption,unsigned char *p_outbuf*/)
{
    //printf("in function %s\n",__FUNCTION__);
    u_int64_t ntpTimestamp = TimestampToNtp( pts );
    u_int8_t* pData = (u_int8_t*)buffer;
    u_int32_t bytesToSend = data_len;
    struct iovec iov;
    u_int32_t plen=0;
    u_int8_t *pBuf=NULL;
    int snd_cnt = 0;
    int bytes_out = 0;
    //LOGE("to call rtp_update");
    rtp_update(video_rtp_session);
    //LOGE("called rtp_update");
    if(bytesToSend>MAX_RTPSIZE)
    {
    	unsigned char nalu_head = pData[0];
	pData=pData+1;
	bytesToSend--;
	int n=bytesToSend/(MAX_RTPSIZE-2);
	int mod=bytesToSend%(MAX_RTPSIZE-2);
	if(mod==0)
	    pBuf=(u_int8_t *)malloc(sizeof(u_int8_t)*(bytesToSend+2*n));
	else if(mod>0)
	    pBuf=(u_int8_t *)malloc(sizeof(u_int8_t)*(bytesToSend+2*n+2));
	int i;
	for(i=0;i<=n;i++)
	{
	    if(i==0)
	    {
		pBuf[plen++]=(nalu_head&0x60)|0x1c;//0x5c;
		pBuf[plen++]=0x80 |(nalu_head & 0X1f);
		memcpy(pBuf+plen,pData+(MAX_RTPSIZE-2)*i,(MAX_RTPSIZE-2));
		plen+=(MAX_RTPSIZE-2);
	    }
	    else if(i==n)
	    {
		if(mod==0)
		    break;
		pBuf[plen++]=(nalu_head&0x60)|0x1c;//0x5c;
		pBuf[plen++]=0x40 |(nalu_head & 0X1f) ;
		memcpy(pBuf+plen,pData+(MAX_RTPSIZE-2)*i,bytesToSend-(MAX_RTPSIZE-2)*i);
		plen+=(bytesToSend-(MAX_RTPSIZE-2)*i);
	    }
	    else
	    {
		if(i==n-1&&mod==0)

		{
		    pBuf[plen++]=(nalu_head&0x60)|0x1c;//0x5c;
		    pBuf[plen++]=0x40 |(nalu_head & 0X1f);
		    memcpy(pBuf+plen,pData+(MAX_RTPSIZE-2)*i,(MAX_RTPSIZE-2));
		    plen+=(MAX_RTPSIZE-2);
		}
		else
		{
		    pBuf[plen++]=(nalu_head&0x60)|0x1c;//0x5c;
		    pBuf[plen++]=0x00 |(nalu_head & 0X1f);
		    memcpy(pBuf+plen,pData+(MAX_RTPSIZE-2)*i,(MAX_RTPSIZE-2));
		    plen+=(MAX_RTPSIZE-2);
		}
	    }
	}

	bytesToSend=plen;
    }

    else

    {
	pBuf=(u_int8_t *)malloc(sizeof(u_int8_t)*bytesToSend);
	memcpy(pBuf,pData,bytesToSend);
    }
    u_int8_t * pTemp=pBuf;
    while (bytesToSend) {
	u_int32_t payloadLength;
	int lastPacket;
	if (bytesToSend <= MAX_RTPSIZE){
	    payloadLength = bytesToSend;
	    lastPacket = mark;
	} else {
	    payloadLength = MAX_RTPSIZE;
	    lastPacket = 0;
	}
	snd_cnt ++;
	if(snd_cnt %2 == 0)
	{
		//LOGE("AAA  Called SleepMs ,snd_cnt = %d",snd_cnt);
		//SleepMs(5);
		//LOGE("BBB Called SleepMs ,snd_cnt = %d",snd_cnt);
	}
	//*pTemp = 0x41;
	if(buffer[0] == 0x67||buffer[0] == 0x68)
	{
		lastPacket = 1;
	}
	//                //memcpy(pTemp+1, pData, payloadLength);
	#if 0
	if(p_encryption != NULL && p_outbuf !=NULL)
	{
		p_encryption->encrypt(0, pBuf,p_outbuf, payloadLength, &bytes_out);
		iov.iov_base = p_outbuf;
		iov.iov_len = bytes_out;
	}
	else
		#endif
	{
		iov.iov_base = pBuf;//pTemp;
		iov.iov_len = payloadLength;
	}
	//LOGE("to call rtp_send_data_iov\n");
	rtp_send_data_iov(video_rtp_session, pts,96, lastPacket, 0, NULL, &iov, 1, NULL, 0, 0,0);
	//LOGE("have called rtp_send_data_iov");
	//printf("num=%d\n",++num);
	fflush(stdout);
	pBuf += payloadLength;
	bytesToSend -= payloadLength;
    }
    //LOGE("sended pkg snd_cnt = %d",snd_cnt);
    free(pTemp);
}



int VideoPlayer::RtpToNalu(unsigned char * pRtpHeader, size_t rtp_packet_len_with_header)
{
	int bytes_out = 0;
	if(rtp_packet_len_with_header <=RTP_HDR_LEN)
	{
		LOG_PLAY("RTP packet length too short,ignored");
		return -1;
	}
	unsigned char  media_type = pRtpHeader[1]&0x7F;
	bool is_mark = pRtpHeader[1]&0x80;
	unsigned int ts = *( (uint32_t*)(pRtpHeader + 4) );
	ts = ntohl(ts);
	unsigned short rtp_seq =  *( (uint16_t*)(pRtpHeader + 2) );
	rtp_seq = ntohs(rtp_seq);
	unsigned char nal_type = 0;
	unsigned char   nal_data_offset = RTP_HDR_LEN;
	LOG_PLAY("new rtp seq:%d,old rtp seq:%d",rtp_seq,m_oldrtpseq);
	if(rtp_seq != (m_oldrtpseq+1)%65536)
	{
		m_hasIFrame = false;
		m_NalLen = 0;
		LOG_PLAY("video player seq disorder eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
	}
	m_oldrtpseq = rtp_seq;
	int ret = 0;
	if(RTP_PT_VIDEO == media_type)
	{
		LOG_PLAY("to process video rtp packet\n");
		nal_type = pRtpHeader[RTP_HDR_LEN]&0x1f;
		if (nal_type > 0 && nal_type < 24)
		{
			m_FrameFlag =0;
            m_Buf[m_NalLen++] = 0x00;
			m_Buf[m_NalLen++] = 0x00;
			m_Buf[m_NalLen++] = 0x00;
			m_Buf[m_NalLen++] = 0x01;
            memcpy (m_Buf+m_NalLen,pRtpHeader +nal_data_offset, rtp_packet_len_with_header -nal_data_offset);
			m_naltype = m_Buf[4]&0x1f;
			if(!m_hasIFrame)
			{
				m_hasIFrame = (m_naltype == 5);
			}
            m_NalLen  += rtp_packet_len_with_header -nal_data_offset;
			if(is_mark)
			{
				if(m_naltype == 7 || m_naltype == 8 || m_hasIFrame){
					ret = process_nalu( ts,m_Buf,m_NalLen);
				}
				m_NalLen = 0;
			}
			m_rtpseq = rtp_seq;
            return ret;
		}
		if(nal_type == 28) // FU-A (fragmented nal)
		{
                /*
                0                   1                   2                   3
                0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                | FU indicator  |   FU header   |                               |
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
                |                                                               |
                |                         FU payload                            |
                |                                                               |
                |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |                               :...OPTIONAL RTP padding        |
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                */
           // these are the same as above, we just redo them here for clarity...
             unsigned char fu_indicator = *(pRtpHeader +nal_data_offset);
             // skip the fu_indicator
             nal_data_offset ++;
			 unsigned char fu_header  = *(pRtpHeader +nal_data_offset);   // read the fu_header.
			 unsigned char start_bit  = fu_header >> 7;
			 unsigned char end_bit   = (fu_header >> 6) & 1;
			 unsigned char reconstructed_nal;
			 // reconstruct this packet's true nal; only the data follows..
			 reconstructed_nal = fu_indicator & 0xe0;  // the original nal forbidden bit and NRI are stored in this packet's nal;
             reconstructed_nal |= fu_header&0x1f;
			 // skip the fu_header...
			 nal_data_offset ++;
			 if(start_bit)
			 {
			 	m_Buf[m_NalLen++] = 0x00;
				m_Buf[m_NalLen++] = 0x00;
				m_Buf[m_NalLen++] = 0x00;
				m_Buf[m_NalLen++] = 0x01;
				m_Buf[m_NalLen++] = reconstructed_nal;
				m_naltype = m_Buf[4]&0x1f;
				if(!m_hasIFrame)
				{
					m_hasIFrame = (m_naltype == 5);
				}
		   		m_FrameFlag=1;
		   		m_rtpseq = rtp_seq ;
				memcpy (m_Buf+m_NalLen,pRtpHeader +nal_data_offset, rtp_packet_len_with_header -nal_data_offset);
				m_NalLen += (rtp_packet_len_with_header -nal_data_offset);
			 }
			 else
			 {
			 	if(m_FrameFlag == 0)
		  		{
		  			m_NalLen=0;
					return 0;
		  		}
	         	else
		  		{
		  			if((m_rtpseq+1)%65536 != rtp_seq)
					{
						m_NalLen=0;
						m_FrameFlag =0;
						return 0;
					}
		  		}
		   		m_rtpseq++;
                memcpy (m_Buf+m_NalLen,pRtpHeader +nal_data_offset, rtp_packet_len_with_header -nal_data_offset);
				m_NalLen += (rtp_packet_len_with_header -nal_data_offset);
			}

			if(end_bit&&m_FrameFlag ==1 && is_mark)
			{
				if(m_naltype == 7 || m_naltype == 8 || m_hasIFrame){
					ret = process_nalu(ts, m_Buf, m_NalLen);
				}
				m_NalLen = 0;
			}
			if(end_bit)
			{
				m_FrameFlag =0;
			}
			return ret;
		}
	}
	else
	{
	 	LOG_PLAY("only process video rtp packet,not support others");
	}
	return 0;
}
#endif
