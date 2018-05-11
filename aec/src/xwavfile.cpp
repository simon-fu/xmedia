#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "xcutil.h"
#include "xwavfile.h"




static
int _sys_is_littel_endian(){
    union _byteorder{
        short s;
        char c[2];
    }un;
    un.s = 0x0102;
    return (un.c[0] == 2 && un.c[1] == 1);
}

static inline
bool sys_is_littel_endian(){
    static int order = -1;
    if(order == -1){
        order = _sys_is_littel_endian();
    }
    return order;
}


struct wavfile_reader_st{
    FILE * fp;
    wavfileinfo fileinfo;
};


unsigned int wavfile_reader_samplerate(wavfile_reader_t obj){
    return obj->fileinfo.samplerate;
}

unsigned int wavfile_reader_channels(wavfile_reader_t obj){
    return obj->fileinfo.channels;
}

unsigned int wavfile_reader_samplebits(wavfile_reader_t obj){
    return obj->fileinfo.samplebits;
}

// WAVE file format see http://soundfile.sapp.org/doc/WaveFormat/

static int wavfile_reader_read_header(wavfile_reader_t obj){
    int ret = 0;
    do{
        wavfileinfo * fileinfo = &obj->fileinfo;
        
        unsigned char buf[128];
        
        // "RIFF" chunk
        
        ret = fread(buf, sizeof(char), 12, obj->fp);
        if(ret != 12){
            ret = -11;
            break;
        }
        
        unsigned char * chunkId = buf + 0;
        if(memcmp(chunkId, "RIFF", 4) != 0){
            ret = -12;
            break;
        }
        
        unsigned char * waveChunkId = buf + 8;
        if(memcmp(waveChunkId, "WAVE", 4) != 0){
            ret = -13;
            break;
        }
        
        fileinfo->chunkSize = le_get_u32(buf + 4);
        // dbgi("chunk size %d", fileinfo->chunkSize);
        
        
        // "fmt" sub-chunk
        
        ret = fread(buf, sizeof(char), 8, obj->fp);
        if(ret != 8){
            ret = -14;
            break;
        }
        
        unsigned char * subchunk1Id = buf + 0;
        if(memcmp(subchunk1Id, "fmt ", 4) != 0){
            ret = -15;
            break;
        }
        
        unsigned int subchunk1Size = le_get_u32(buf + 4);
        if(subchunk1Size < 16){
            ret = -16;
            break;
            
        }
        // dbgi("subchunk1Size %d", subchunk1Size);
        
        ret = fread(buf, sizeof(char), subchunk1Size, obj->fp);
        if(ret != subchunk1Size){
            ret = -17;
            break;
        }
        
        fileinfo->format = le_get_u16(buf + 0);
        fileinfo->channels = le_get_u16(buf + 2);
        fileinfo->samplerate = le_get_u32(buf + 4);
        fileinfo->byterate = le_get_u32(buf + 8);
        fileinfo->blockalign = le_get_u16(buf + 12);
        fileinfo->samplebits = le_get_u16(buf + 14);
        
        // dbgi("read wav: fmt=%d, ch=%d, Hz=%d, byterate=%d, align=%d, bits=%d"
        //     , fileinfo->format
        //     , fileinfo->channels
        //     , fileinfo->samplerate
        //     , fileinfo->byterate
        //     , fileinfo->blockalign
        //     , fileinfo->samplebits);
        
        if(fileinfo->format != 1){ // NOT PCM
            ret = -18;
            break;
        }
        
        
        // "data" sub-chunk
        
        ret = fread(buf, sizeof(char), 8, obj->fp);
        if(ret != 8){
            ret = -14;
            break;
        }
        
        unsigned char * subchunk2Id = buf + 0;
        if(memcmp(subchunk2Id, "data ", 4) != 0){
            ret = -15;
            break;
        }
        
        fileinfo->dataChunkSize = le_get_u32(buf + 4);
        // dbgi("data sub-chunk size %d", fileinfo->dataChunkSize);
        
        
        ret = 0;
        
    }while(0);
    return ret;
}

void wavfile_reader_close(wavfile_reader_t obj){
    if(!obj) return;
    if(obj->fp){
        fclose(obj->fp);
        obj->fp = NULL;
    }
    free(obj);
}

wavfile_reader_t wavfile_reader_open(const char * filename){
    int ret = 0;
    wavfile_reader_t obj = NULL;
    do{
        obj = (wavfile_reader_t)malloc(sizeof(struct wavfile_reader_st));
        memset(obj, 0, sizeof(struct wavfile_reader_st));
        obj->fp = fopen(filename, "rb");
        if(!obj->fp){
            ret = -1;
            break;
        }
        
        ret = wavfile_reader_read_header(obj);
        if(ret){
            // dbge("wavfile_reader_read_header fail ret=%d", ret);
            ret = -2;
            break;
        }
        
        ret = 0;
        
    }while(0);
    if(ret){
        wavfile_reader_close(obj);
        obj = NULL;
    }
    return obj;
}

int wavfile_reader_read(wavfile_reader_t obj, void * data, int length){
    int ret = fread(data, sizeof(char), length, obj->fp);
    return ret;
}

int wavfile_reader_read_short(wavfile_reader_t obj, short * data, int length){
    int ret = fread(data, sizeof(short), length, obj->fp);
    if(ret <= 0){
        return ret;
    }
    int samples = ret;
    
    if(!sys_is_littel_endian()){
        for(int i = 0; i < samples; i++){
            unsigned char * p = (unsigned char *)&data[i];
            short sample = le_get_u32(p);
            data[i] = sample;
        }
    }
    
    return samples;
}

int wavfile_reader_info(wavfile_reader_t obj, wavfileinfo * info){
    if(info){
        *info = obj->fileinfo;
    }
    return 0;
}




struct wavfile_writer_st{
    FILE * fp;
    wavfileinfo fileinfo;
    int last_ret;
};



static int wavfile_writer_write_header(wavfile_writer_t obj){
    int ret = 0;
    do{
        wavfileinfo * fileinfo = &obj->fileinfo;
        fileinfo->chunkSize = 36 + fileinfo->dataChunkSize;
        
        unsigned char buf[128];
        int len = 0;
        
        // "RIFF" chunk
        
        fseek(obj->fp, 0, SEEK_SET);
        
        len = 0;
        
        memcpy(buf+len, "RIFF", 4);
        len += 4;
        
        le_set_u32(fileinfo->chunkSize, buf+len);
        len += 4;
        
        memcpy(buf+len, "WAVE", 4);
        len += 4;
        
        ret = fwrite(buf, sizeof(char), len, obj->fp);
        if(ret != len){
            ret = -51;
            break;
        }
        
        
        // "fmt" sub-chunk
        
        len = 0;
        
        memcpy(buf+len, "fmt ", 4);
        len += 4;
        
        le_set_u32(16, buf+len);
        len += 4;
        
        
        le_set_u16(fileinfo->format, buf+len);
        len += 2;
        
        le_set_u16(fileinfo->channels, buf+len);
        len += 2;
        
        le_set_u32(fileinfo->samplerate, buf+len);
        len += 4;
        
        le_set_u32(fileinfo->byterate, buf+len);
        len += 4;
        
        le_set_u16(fileinfo->blockalign, buf+len);
        len += 2;
        
        le_set_u32(fileinfo->samplebits, buf+len);
        len += 2;
        
        ret = fwrite(buf, sizeof(char), len, obj->fp);
        if(ret != len){
            ret = -52;
            break;
        }
        
        
        
        // "data" sub-chunk
        
        len = 0;
        
        memcpy(buf+len, "data", 4);
        len += 4;
        
        le_set_u32(fileinfo->dataChunkSize, buf+len);
        len += 4;
        
        ret = fwrite(buf, sizeof(char), len, obj->fp);
        if(ret != len){
            ret = -53;
            break;
        }
        
        ret = 0;
        
    }while(0);
    
    obj->last_ret = ret;
    
    return ret;
}

void wavfile_writer_close(wavfile_writer_t obj){
    if(!obj) return;
    if(obj->fp){
        if(obj->last_ret == 0){
            wavfile_writer_write_header(obj);
        }
        
        fclose(obj->fp);
        obj->fp = NULL;
    }
    free(obj);
}

wavfile_writer_t wavfile_writer_open(const char * filename, unsigned int channels, unsigned int samplerate){
    int ret = 0;
    wavfile_writer_t obj = NULL;
    do{
        obj = (wavfile_writer_t)malloc(sizeof(struct wavfile_writer_st));
        memset(obj, 0, sizeof(struct wavfile_writer_st));
        obj->fp = fopen(filename, "wb");
        if(!obj->fp){
            ret = -1;
            break;
        }
        obj->fileinfo.channels = channels;
        obj->fileinfo.samplerate = samplerate;
        
        obj->fileinfo.format = 1;
        obj->fileinfo.samplebits = 16;
        obj->fileinfo.byterate = obj->fileinfo.samplerate * 2 * obj->fileinfo.channels;
        obj->fileinfo.blockalign = 2 * obj->fileinfo.channels;
        obj->fileinfo.chunkSize = 0;
        obj->fileinfo.dataChunkSize = 0;
        
        ret = wavfile_writer_write_header(obj);
        if(ret){
            // dbge("wavfile_writer_read_header fail ret=%d", ret);
            ret = -2;
            break;
        }
        
        ret = 0;
        
    }while(0);
    
    obj->last_ret = ret;
    
    if(ret){
        wavfile_writer_close(obj);
        obj = NULL;
    }
    return obj;
}

wavfile_writer_t wavfile_writer_open_with_reader(const char * filename, wavfile_reader_t reader){
    return wavfile_writer_open(filename, reader->fileinfo.channels, reader->fileinfo.samplerate);
}

// wavfile_writer_t wavfile_writer_open_with_reader(const char * filename, wavfile_reader_t reader){
//     int ret = 0;
//     wavfile_writer_t obj = NULL;
//     do{
//         obj = (wavfile_writer_t)malloc(sizeof(struct wavfile_writer_st));
//         memset(obj, 0, sizeof(struct wavfile_writer_st));
//         obj->fp = fopen(filename, "wb");
//         if(!obj->fp){
//             ret = -1;
//             break;
//         }
//         obj->fileinfo = reader->fileinfo;
//         obj->fileinfo.chunkSize = 0;
//         obj->fileinfo.dataChunkSize = 0;

//         ret = wavfile_writer_write_header(obj);
//         if(ret){
//             // dbge("wavfile_writer_read_header fail ret=%d", ret);
//             ret = -2;
//             break;
//         }

//         ret = 0;

//     }while(0);

//     obj->last_ret = ret;

//     if(ret){
//         wavfile_writer_close(obj);
//         obj = NULL;
//     }
//     return obj;
// }

int wavfile_writer_info(wavfile_writer_t obj, wavfileinfo * info){
    if(info){
        *info = obj->fileinfo;
    }
    return 0;
}

int wavfile_writer_write(wavfile_writer_t obj, void * data, int length){
    int ret = fwrite(data, sizeof(char), length, obj->fp);
    if(ret != length){
        return -1;
    }
    obj->fileinfo.dataChunkSize += length;
    return 0;
}

int wavfile_writer_write_short(wavfile_writer_t obj, const short * data, int length){
    if(!sys_is_littel_endian()){
        unsigned char buf2[2];
        for(int i = 0; i < length; i++){
            short sample = data[i];
            le_set_u16(sample, buf2);
            int ret = fwrite(buf2, sizeof(char), 2, obj->fp);
            if(ret <= 0){
                return ret;
            }
            obj->fileinfo.dataChunkSize += 2;
        }
        return length;
    }else {
        int ret = fwrite(data, sizeof(short), length, obj->fp);
        if(ret != length){
            return -1;
        }
        obj->fileinfo.dataChunkSize += (length * sizeof(short));
        return ret;
    }
}

int wavfile_writer_write_vectors(wavfile_writer_t obj, int length, int num_vec, short * const vec[]){
    
    for(int i = 0; i < length; i++){
        unsigned char buf2[2];
        for(int vi = 0; vi < num_vec; vi++){
            const short * d = vec[vi];
            short sample = d[i];
            le_set_u16(sample, buf2);
            int ret = fwrite(buf2, sizeof(char), 2, obj->fp);
            if(ret <= 0){
                return ret;
            }
            obj->fileinfo.dataChunkSize += 2;
        }
    }
    return length;
}

// int wavfile_writer_write_short_var(wavfile_writer_t obj, int length, const short * data, ...){

//     va_list pArgs;
//     va_start(pArgs, data);

//     std::vector<const short *> vec;
//     vec.push_back(data);
//     while(1){
//         const short * d = va_arg(pArgs, const short *);
//         if(!d) break;
//         vec.push_back(d);
//     }
//     va_end(pArgs);

//     for(int i = 0; i < length; i++){
//         unsigned char buf2[2];
//         for(auto d :vec){
//             short sample = d[i];
//             le_set_u16(sample, buf2);
//             int ret = fwrite(buf2, sizeof(char), 2, obj->fp);
//             if(ret <= 0){
//                 return ret;
//             }
//             obj->fileinfo.dataChunkSize += 2;
//         }
//     }
//     return length;
// }





