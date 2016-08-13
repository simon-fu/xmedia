#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h> // SIGINT

#include "webrtc/modules/audio_processing/aecm/echo_control_mobile.h"
// #include "util.h"
#include "xwavfile.h"

#define dbgv(...) do{  printf("<aec>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{  printf("<aec>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{  printf("<aec>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)




// 08-12 12:51:55.198 10459 10683 I alog    : WebRtcAecm_Init: aecmInst=0x610c0a80, sampFreq=8000
// 08-12 12:51:55.198 10459 10683 I alog    : WebRtcAecm_set_config: aecmInst=0x610c0a80, config.cngMode=1, config.echoMode=3
// 08-12 12:51:55.198 10459 10683 I alog    : WebRtcAecm_set_config: aecmInst=0x610c0a80, config.cngMode=0, config.echoMode=3

// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_BufferFarend: aecmInst=0x626f1b18, farend=0x627011b8, nrOfSamples=160
// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_GetBufferFarendError: aecmInst=0x626f1b18, farend=0x627011b8, nrOfSamples=160
// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_BufferFarend: aecmInst=0x626f1b18, farend=0x62701300, nrOfSamples=160
// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_GetBufferFarendError: aecmInst=0x626f1b18, farend=0x62701300, nrOfSamples=160
// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_BufferFarend: aecmInst=0x626f1b18, farend=0x62701448, nrOfSamples=160
// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_GetBufferFarendError: aecmInst=0x626f1b18, farend=0x62701448, nrOfSamples=160

// 08-12 12:42:13.838  9797 10170 I alog    : WebRtcAecm_Process: aecmInst=0x626f1b18, nearendNoisy=0x65193568, nearendClean=0x626f1368, out=0x626f1368, nrOfSamples=160, msInSndCardBuf=150
// 08-12 12:42:13.848  9797 10170 I alog    : WebRtcAecm_Process: aecmInst=0x626f1b18, nearendNoisy=0x65193568, nearendClean=0x626f1368, out=0x626f1368, nrOfSamples=160, msInSndCardBuf=150
// 08-12 12:42:13.858  9797 10170 I alog    : WebRtcAecm_Process: aecmInst=0x626f1b18, nearendNoisy=0x65193568, nearendClean=0x626f1368, out=0x626f1368, nrOfSamples=160, msInSndCardBuf=150
// 08-12 12:42:13.868  9797 10170 I alog    : WebRtcAecm_Process: aecmInst=0x626f1b18, nearendNoisy=0x65193568, nearendClean=0x626f1368, out=0x626f1368, nrOfSamples=160, msInSndCardBuf=150

int main(int argc, char** argv){

	const char * fileNear = "tmp/input.wav"; // "tmp/input.wav"; "tmp/mix.wav";
	const char * fileFar = "tmp/reverse.wav";
	const char * fileOut = "tmp/out.wav";

	int ret = 0;
	wavfile_reader_t readerNear = NULL;
	wavfile_reader_t readerFar = NULL;
	wavfile_writer_t writerOut = NULL;
	void* aecmInst = NULL;
	AecmConfig aecmConfig = {.cngMode=0, .echoMode=3}; 
	int32_t sampFreq = 16000;
	int nrOfSamples = 160;

	// int msInSndCardBuf = 150; // milliseconds
	int msInSndCardBuf = 150; // milliseconds
	// int msInSndCardBuf = 300; // milliseconds

	int dropMilliSeconds = 300;
	
	int16_t near_buf[nrOfSamples];
	int16_t far_buf[nrOfSamples];
	int16_t out_buf[nrOfSamples];

	dbgi("sampFreq = %d", sampFreq);
	dbgi("nrOfSamples = %d", nrOfSamples);
	dbgi("msInSndCardBuf = %d", msInSndCardBuf);
	dbgi("dropMilliSeconds = %d", dropMilliSeconds);

	do{
		readerNear = wavfile_reader_open(fileNear);
		if(!readerNear){
			dbge("fail to open %s", fileNear);
			ret = -1;
			break;
		}
		dbge("successfully open %s", fileNear);

		readerFar = wavfile_reader_open(fileFar);
		if(!readerFar){
			dbge("fail to open %s", fileFar);
			ret = -1;
			break;
		}
		dbge("successfully open %s", fileFar);

		writerOut = wavfile_writer_open_with_reader(fileOut, readerNear);
		if(!writerOut){
			dbge("fail to open %s", fileOut);
			ret = -1;
			break;
		}
		dbge("successfully open %s", fileOut);



		aecmInst = WebRtcAecm_Create();
		if(!aecmInst){
			dbge("WebRtcAecm_Create fail");
			break;
		}
		dbgi("WebRtcAecm_Create success, aecmInst=%p", aecmInst);

		ret = WebRtcAecm_Init(aecmInst, sampFreq);
		if(ret != 0){
			dbge("WebRtcAecm_Init fail ret=%d", ret);
			break;
		}
		dbgi("WebRtcAecm_Init success");

		ret = WebRtcAecm_set_config(aecmInst, aecmConfig);
		if(ret != 0){
			dbge("WebRtcAecm_set_config fail ret=%d, config=[%d, %d]", ret, aecmConfig.cngMode, aecmConfig.echoMode);
			break;
		}
		dbgi("WebRtcAecm_set_config success, config=[.cngMode=%d, .echoMode=%d]", aecmConfig.cngMode, aecmConfig.echoMode);


		int framebytes = nrOfSamples * sizeof(int16_t);
		int frameCount = 0;

		int samplerate = wavfile_reader_samplerate(readerFar);
		int dropSamples = samplerate * dropMilliSeconds / 1000;
		int dropFrames = (dropSamples + (nrOfSamples-1)) /nrOfSamples;
		// dropFrames *= 2;
		// dropFrames = 30;
		dbgi("drop: samplerate=%d, dropSamples=%d, dropFrames=%d", samplerate, dropSamples, dropFrames);
		while(frameCount < dropFrames){
			ret = wavfile_reader_read(readerFar, far_buf, framebytes);
			if(ret != framebytes){
				dbgi("unexpectd reaching far-end file end");
				ret = -1;
				break;
			}
			ret = 0;
			frameCount++;
		}
		if(ret) break;
		dbgi("dropped frames %d", frameCount);
		
		frameCount = 0;
		while(1){
			ret = wavfile_reader_read(readerFar, far_buf, framebytes);
			if(ret != framebytes){
				dbgi("reach far-end file end");
				ret = 0;
				break;
			}

			ret = WebRtcAecm_BufferFarend(aecmInst, far_buf, nrOfSamples);
			if(ret != 0){
				dbge("WebRtcAecm_BufferFarend fail ret=%d", ret);
				break;
			}

			ret = wavfile_reader_read(readerNear, near_buf, framebytes);
			if(ret != framebytes){
				dbgi("reach near-end file end");
				ret = 0;
				break;
			}

			ret = WebRtcAecm_Process(aecmInst,
			                           near_buf, // nearendNoisy,
			                           near_buf, // nearendClean,
			                           out_buf,
			                           nrOfSamples,
			                           msInSndCardBuf);
			if(ret != 0){
				dbge("WebRtcAecm_Process fail ret=%d", ret);
				break;
			}

			//memcpy(out_buf, near_buf, framebytes);
			ret = wavfile_writer_write(writerOut, out_buf, framebytes);
			if(ret < 0){
				dbge("wavfile_writer_write fail ret=%d", ret);
				break;
			}

			ret = 0;
			frameCount++;
		}

		dbgi("processed frameCount = %d", frameCount);
		if(ret == 0){
			dbgi("process OK");
		}else{
			dbgi("process fail !!!");
		}

	}while(0);

	if(aecmInst){
		dbgi("WebRtcAecm_Free aecmInst=%p", aecmInst);
		WebRtcAecm_Free(aecmInst);
		aecmInst = NULL;
	}

	if(readerNear){
		wavfile_reader_close(readerNear);
		readerNear = NULL;
	}

	if(readerFar){
		wavfile_reader_close(readerFar);
		readerFar = NULL;
	}

	if(writerOut){
		wavfile_writer_close(writerOut);
		writerOut = NULL;
	}

	dbgi("bye!");

}

