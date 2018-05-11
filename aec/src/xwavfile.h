
#ifndef __XWAVFILE_H__
#define __XWAVFILE_H__

#ifdef __cplusplus
extern "C"{
#endif
    
    typedef struct wavfileinfo{
        unsigned int chunkSize;
        
        unsigned int format;
        unsigned int channels;
        unsigned int samplerate;
        unsigned int byterate;
        unsigned int blockalign;
        unsigned int samplebits;
        
        unsigned int dataChunkSize;
    }wavfileinfo;
    
    
    typedef struct wavfile_reader_st * wavfile_reader_t;
    
    wavfile_reader_t wavfile_reader_open(const char * filename);
    
    void wavfile_reader_close(wavfile_reader_t obj);
    
    int wavfile_reader_read(wavfile_reader_t obj, void * data, int length);
    
    int wavfile_reader_read_short(wavfile_reader_t obj, short * data, int length);
    
    int wavfile_reader_info(wavfile_reader_t obj, wavfileinfo * info);
    
    unsigned int wavfile_reader_samplerate(wavfile_reader_t obj);
    
    unsigned int wavfile_reader_channels(wavfile_reader_t obj);
    
    unsigned int wavfile_reader_samplebits(wavfile_reader_t obj);
    
    
    
    typedef struct wavfile_writer_st * wavfile_writer_t;
    
    wavfile_writer_t wavfile_writer_open_with_reader(const char * filename, wavfile_reader_t reader);
    
    wavfile_writer_t wavfile_writer_open(const char * filename, unsigned int channels, unsigned int samplerate);
    
    void wavfile_writer_close(wavfile_writer_t obj);
    
    int wavfile_writer_info(wavfile_writer_t obj, wavfileinfo * info);
    
    int wavfile_writer_write(wavfile_writer_t obj, void * data, int length);
    
    int wavfile_writer_write_short(wavfile_writer_t obj, const short * data, int length);
    
    int wavfile_writer_write_vectors(wavfile_writer_t obj, int length, int num_vec, short * const vec[]);
    
    
    
#ifdef __cplusplus
}
#endif

#endif

