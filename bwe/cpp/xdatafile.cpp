
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <deque>
#include <utility>
#include <vector>
#include <math.h>
#include <algorithm> // std::max

#include "xdatafile.h"

#define dbgv(...) do{  printf("<xdata>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{  printf("<xdata>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{  printf("<xdata>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)


void xvector_free(xvector * vector){
    if(vector->data){
        free(vector->data);
        vector->data  = NULL;
    }
}

void xvector_array_free(xvector_array * arr){
	if(!arr) return;
    if(arr->vectors){
        for(int i = 0; i < arr->num_vectors; i++){
            xvector_free(&arr->vectors[i]);
        }
        free(arr->vectors);
        arr->vectors = NULL;
    }
    free(arr);
}



#define is_c_blank(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n' || (c) == ',' )
static
char * next_non_blank(char *p){
    while(*p != '\0' && is_c_blank(*p)){
        p++;
    }
    return p;
}

static
char * next_blank(char *p){
    while(*p != '\0' && (!is_c_blank(*p))){
        p++;
    }
    return p;
}

static
char * next_line_end(char * p){
    while(*p != '\0' && *p != '\r' && *p != '\n'){
        p++;
    }
    return p;
}

static 
int detect_words(const char * buf){
    int num_words = 0;
    const char * p = buf;
    int last_blank = 1;
    while(*p != '\0'){
        char c = *p;
        if(is_c_blank(c)){
            last_blank = 1;
        }else{
            if(last_blank){
                ++num_words;
            }
            last_blank = 0;
        }
        p++;
    }

    return num_words;
}

typedef struct line_parser{
    char * p;
    char * next;
}line_parser;

void line_parser_init(line_parser * parser, char * buf){
    memset(parser, 0, sizeof(line_parser));
    char * pend = next_line_end(buf);
    *pend = '\0';
    parser->p = buf;
}

char * line_parser_next(line_parser * parser){
    char *p = next_non_blank(parser->p);
    if(!(*p)){
        return NULL;
    }
    parser->next = next_blank(p);
    *parser->next = '\0';
    parser->p = ++parser->next;
    return p;  
}



xvector_array * load_txt(const char * filename){
    FILE * fp = NULL;
    int ret = -1;
    char buf[1002];
    int num_lines = 0;
    int num_cols = 0;
    xvector_array * arr = 0;

    do{
        
        fp = fopen(filename, "r");
        if(!fp){
            dbge("fail to read open : [%s]", filename);
            break;
        }
        // dbgi("read opened: [%s]", filename);

        arr = (xvector_array *) malloc(sizeof(xvector_array));
        memset(arr, 0, sizeof(xvector_array));

        num_lines = 0;
        while (fgets(buf, 1000, fp)){
            num_lines++;
        }
        if(num_lines <= 1){
            dbge("file zero lines, [%s]", filename);
            break;
        }
        num_lines--;
        dbgi("lines: %d", num_lines);

        rewind(fp);
        fgets(buf, 1000, fp);
        num_cols = detect_words(buf);
        if(num_cols == 0){
            dbge("file zero cols, [%s]", filename);
            break;
        }
        dbgi("cols: %d", num_cols);

        arr->num_vectors = num_cols;
        arr->vectors = (xvector *) malloc(arr->num_vectors * sizeof(xvector));

        // dbgi("header=[%s]", buf);
        line_parser parser;
        line_parser_init(&parser, buf);
        num_cols = 0;        
        char * p = line_parser_next(&parser);
        while(p) {
            dbgi("load vectors[%d]=[%s]", num_cols, p);
            strcpy(arr->vectors[num_cols].name, p);
            arr->vectors[num_cols].num_rows = num_lines;
            arr->vectors[num_cols].data = (xdata_item *) malloc(num_lines * sizeof(xdata_item));
            num_cols++;
            p = line_parser_next(&parser);
        }

        
        double d;
        ret = 0;
        
        for(int row = 0; row < num_lines; row++){
            fgets(buf, 1000, fp);
            // dbgi("read row %d: [%s]", row, buf);

            line_parser_init(&parser, buf);
            char * p = line_parser_next(&parser);
            int col = 0;
            while(p) {
                if(*p == '-' && *(p+1) == '\0' ){
                    arr->vectors[col].data[row].row = -1;
                }else{
                    arr->vectors[col].data[row].row = row;
                    arr->vectors[col].data[row].value = atof(p);
                }
                col++;
                p = line_parser_next(&parser);
            }
            if(col != num_cols){
                dbge("fail to read at [%d][%d], [%s]", row, col, buf);
                ret = -1;
                break;
            }
        }
        if(ret < 0) break;

        ret = 0;
    }while(0);
    if(fp){
        fclose(fp);
        fp = NULL;
    }
    if(ret && arr){
        xvector_array_free(arr);
        arr = NULL;
    }
    return arr;
}
