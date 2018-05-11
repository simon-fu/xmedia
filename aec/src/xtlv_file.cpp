
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "xtlv_file.h"
#include "xcutil.h"

struct tlv_file_st {
    FILE * fp;
    
};

int tlv_file_write(tlv_file_t obj, int type, int length, void * value){
    // unsigned short t16;
    // t16 = (unsigned short)type;
    fwrite(&type,    1, 4, obj->fp); // type
    fwrite(&length, 1, 4, obj->fp); // length
    if(length > 0){
        fwrite(value,   1, length, obj->fp); // value
    }
    return 0;
}

int tlv_file_write2(tlv_file_t obj, int type, int length1, void * value1, int length2, void * value2){
    // unsigned short t16;
    // t16 = (unsigned short)type;
    int length = length1 + length2;
    fwrite(&type,    1, 4, obj->fp); // type
    fwrite(&length, 1, 4, obj->fp); // length
    if(length1 > 0){
        fwrite(value1,   1, length1, obj->fp); // value1
    }
    if(length2 > 0){
        fwrite(value2,   1, length2, obj->fp); // value2
    }
    
    return 0;
}


void tlv_file_close(tlv_file_t obj){
    if(!obj) return;
    if(obj->fp){
        fclose(obj->fp);
        obj->fp = NULL;
    }
    free(obj);
}

tlv_file_t tlv_file_open(const char * filename){
    int ret = 0;
    tlv_file_t obj = NULL;
    do{
        obj = (tlv_file_t)malloc(sizeof(struct tlv_file_st));
        memset(obj, 0, sizeof(struct tlv_file_st));
        obj->fp = fopen(filename, "wb");
        if(!obj->fp){
            ret = -1;
            break;
        }
        
        ret = 0;
        
    }while(0);
    if(ret){
        tlv_file_close(obj);
        obj = NULL;
    }
    return obj;
}

int tlv_file_read_raw(FILE * fp, void * buf, int bufsize, int * plength, int * ptype){
    int ret = 0;
    int type;
    int length;
    int bytes;
    unsigned char t4[4];
    
    do
    {
        bytes = fread(t4, sizeof(char), 4, fp);
        if(bytes != 4){
            ret = -11;
            break;
        }
        type = le_get_u32(t4);
        // dbgi("tlv: type=%d\n", type);
        
        bytes = fread(t4, sizeof(char), 4, fp);
        if(bytes != 4){
            ret = -12;
            break;
        }
        length = le_get_u32(t4);
        
        if(length > bufsize){
            // printf("too big tlv length %d, bufsize=%d\n", length, bufsize);
            ret = -1000;
            break;
        }
        
        bytes = fread(buf, sizeof(char), length, fp);
        if(bytes != length){
            ret = -13;
            break;
        }
        
        *ptype = type;
        *plength = length;
        
        ret = 0;
    } while (0);
    return ret;
}

