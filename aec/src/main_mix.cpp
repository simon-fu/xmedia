#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h> // SIGINT

#include "webrtc/modules/audio_processing/aecm/echo_control_mobile.h"
// #include "util.h"
#include "xwavfile.h"

#define dbgv(...) do{  printf("<mix>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{  printf("<mix>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{  printf("<mix>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)



int main(int argc, char** argv){

	const char * file1 = "tmp/input.wav";
	const char * file2 = "audio_long16.wav";
	const char * fileOut = "tmp/mix.wav";

	int ret = 0;
	wavfile_reader_t reader1 = NULL;
	wavfile_reader_t reader2 = NULL;
	wavfile_writer_t writerOut = NULL;


	do{
		reader1 = wavfile_reader_open(file1);
		if(!reader1){
			dbge("fail to open %s", file1);
			ret = -1;
			break;
		}
		dbge("successfully open file1 => %s", file1);

		reader2 = wavfile_reader_open(file2);
		if(!reader2){
			dbge("fail to open %s", file2);
			ret = -1;
			break;
		}
		dbge("successfully open file2 => %s", file2);

		writerOut = wavfile_writer_open_with_reader(fileOut, reader1);
		if(!writerOut){
			dbge("fail to open %s", fileOut);
			ret = -1;
			break;
		}
		dbge("successfully open fileout => %s", fileOut);

		wavfileinfo info1;
		wavfileinfo info2;

		wavfile_reader_info(reader1, &info1);
		wavfile_reader_info(reader2, &info2);

		if(info1.channels != info2.channels
			|| info1.samplerate != info2.samplerate
			|| info1.samplebits != info2.samplebits){
			dbge("audio parameter difference !!!");
			ret = -1;
			break;
		}

		int samplerate = info1.samplerate;
		int nrOfSamples = info1.samplerate/100;
		int framebytes = nrOfSamples * sizeof(int16_t);
		int frameCount = 0;

		int16_t * buf1 = (int16_t *) malloc(nrOfSamples*sizeof(int16_t));
		int16_t * buf2 = (int16_t *) malloc(nrOfSamples*sizeof(int16_t));
		int16_t * bufout = (int16_t *) malloc(nrOfSamples*sizeof(int16_t));
		while(1){
			ret = wavfile_reader_read(reader1, buf1, framebytes);
			if(ret != framebytes){
				dbgi("reach file1 end");
				ret = 0;
				break;
			}

			ret = wavfile_reader_read(reader2, buf2, framebytes);
			if(ret != framebytes){
				dbgi("reach file2 end");
				ret = -1;
				break;
			}

			for(int i = 0; i < nrOfSamples; i++){
				bufout[i] = buf1[i] + buf2[i];
			}

			ret = wavfile_writer_write(writerOut, bufout, framebytes);
			if(ret < 0){
				dbge("wavfile_writer_write fail ret=%d", ret);
				break;
			}


			ret = 0;

			frameCount++;
		}
		free(buf1);
		free(buf2);
		free(bufout);


		dbgi("processed frameCount = %d", frameCount);
		if(ret == 0){
			dbgi("process OK");
		}else{
			dbgi("process fail !!!");
		}

	}while(0);

	if(reader1){
		wavfile_reader_close(reader1);
		reader1 = NULL;
	}

	if(reader2){
		wavfile_reader_close(reader2);
		reader2 = NULL;
	}

	if(writerOut){
		wavfile_writer_close(writerOut);
		writerOut = NULL;
	}

	dbgi("bye!");

}

