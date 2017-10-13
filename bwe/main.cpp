
#include <stdio.h>
#include <stdlib.h>
#include "kalman_filter.h" 
//#include "fixed_test1.h"
//#include "fixed_test2.h"
// #include "free_test07.h"

#define dbgv(...) do{  printf("<main>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{  printf("<main>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{  printf("<main>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)

typedef float kalman_data_t;
struct array2d{
    kalman_data_t * d;
    int rows;
    int cols;
    kalman_data_t& at(int row, int col){
        return this->d[row * cols + col];
    }
};


#define is_c_blank(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')
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

static
int load_txt(const char * filename, array2d *data){
    FILE * fp = NULL;
    // kalman_data_t * data = NULL;
    int ret = -1;
    char buf[1002];
    int num_lines = 0;
    int num_cols = 0;
    do{
        fp = fopen(filename, "r");
        if(!fp){
            dbge("fail to read open : [%s]", filename);
            break;
        }
        // dbgi("read opened: [%s]", filename);

        num_lines = 0;
        while (fgets(buf, 1000, fp)){
            num_lines++;
        }
        if(num_lines == 0){
            dbge("file zero lines, [%s]", filename);
            break;
        }
        // dbgi("lines: %d", num_lines);

        num_cols = detect_words(buf);
        if(num_cols == 0){
            dbge("file zero cols, [%s]", filename);
            break;
        }
        // dbgi("cols: %d", num_cols);
        data->rows = num_lines;
        data->cols = num_cols;
        data->d = (kalman_data_t *)malloc(num_lines * num_cols * sizeof(kalman_data_t));
        rewind(fp);
        double d;
        ret = 0;
        
        for(int row = 0; row < num_lines; row++){
            fgets(buf, 1000, fp);
            // dbgi("read row %d: [%s]", row, buf);

            char * pend = next_line_end(buf);
            *pend = '\0';
            // dbgi("read row %d: [%s]", row, buf);

            char * p = buf;
            for(int col = 0; col < num_cols; col++){
                p = next_non_blank(p);
                char * next = next_blank(p);
                if(!(*p)){
                    dbge("fail to read at [%d][%d], [%s]", row, col, buf);
                    ret = -1;
                    break;
                }
                *next = '\0';
                d = atof(p);
                p = ++next;
                // dbgi("read at [%d][%d], d=%f", row, col, d);
                // data[row * num_cols + col] = (kalman_data_t)d;
                data->at(row, col) = (kalman_data_t)d;
            }
            if(ret < 0) break;
        }
        if(ret < 0) break;


        // if(prows) *prows = num_lines;
        // if(pcols) *pcols = num_cols;
        ret = 0;
    }while(0);
    if(fp){
        fclose(fp);
        fp = NULL;
    }
    if(ret && data->d){
        free(data->d);
        data->d = NULL;
    }
    return ret;
}



int main(void){
    const char * filename = "sendside_20171011_172641_0x1014508e8.txt";
    array2d datastor;
    array2d *data = &datastor;

    int ret = load_txt(filename, data);
    if(ret < 0){
        return -1;    
    }
    dbgi("loaded [%s]", filename); 
    dbgi("rows=%d, cols=%d", data->rows, data->cols); 

    const char * out_filename1 = "out.txt";
    kalman1_state * states1 = NULL;
    kalman2_state * states2 = NULL;
    FILE * fp1 = NULL;
    ret = -1;
    do{
        if(data->cols <= 1){
            dbge("cols <= 1");
            ret = -1;
            break;
        }

        fp1 = fopen(out_filename1, "wb");
        if(!fp1){
            dbge("fail to write open: [%s]", out_filename1);
            ret = -1;
            break;
        }

        int num_measures = (data->cols-1);
        states1 = (kalman1_state *)malloc( num_measures * sizeof(kalman1_state) );
        states2 = (kalman2_state *)malloc( num_measures * sizeof(kalman2_state) );
    
        int index = 1;
        // init kalman by first 2 rows of data
        kalman_data_t z0 = data->at(0, index);
        kalman_data_t z1 = data->at(1, index);

        // kalman1_init(&states1[index], z0, 5e2);
        kalman1_init(&states1[index], 0, 1);

        kalman_data_t init_x[2] = {z0, z1-z0};
        kalman_data_t init_p[2][2] = {{10e-6,0}, {0,10e-6}};
        kalman2_init(&states2[index], init_x, init_p);

        // first 2 rows 
        char dumpbuf[256], dumpbuf2[256];
        int dumplen = 0, dumplen2 = 0;
        dumplen = 0;
        for(int row = 0; row < 2; row++){
            kalman_data_t t = data->at(row, 0);
            kalman_data_t z = data->at(row, index);
            kalman_data_t out1 = kalman1_filter(&states1[index], z);
            kalman_data_t out2 = z;
            dumplen = sprintf(dumpbuf+0, "%.2f %.6f %.6f %.6f\n",t, z, out1, out2);
            fwrite(dumpbuf, dumplen, 1, fp1);
        }

        // other rows
        for(int row = 2; row < data->rows; row++){
            kalman_data_t t = data->at(row, 0);
            kalman_data_t z = data->at(row, index);
            kalman_data_t out1 = kalman1_filter(&states1[index], z);
            kalman_data_t out2 = kalman2_filter(&states2[index], z);
            dumplen = sprintf(dumpbuf+0, "%.2f %.6f %.6f %.6f\n",t, z, out1, out2);
            fwrite(dumpbuf, dumplen, 1, fp1);
        }

        ret = 0;
        dbgi("done");
    }while(0);

    if(fp1){
        fclose(fp1);
        fp1 = NULL;
    }

    if(states1){
        free(states1);
        states1 = NULL;
    }
    if(states2){
        free(states2);
        states2 = NULL;
    }
    if(data->d){
        free(data->d);
        data->d = NULL;
    }

    return 0;
}


#if 0
int main(void)
{
    int i = 0;
    kalman1_state state1;
    float *data = NULL;
    int data_len = 0;
    float out1 = 0;
    /* For 2 Dimension */
    kalman2_state state2;
    float init_x[2];
    float init_p[2][2] = {{10e-6,0}, {0,10e-6}};
    float out2 = 0;

    data = x1;
    data_len = sizeof(x1)/sizeof(float);
    // 1 dimension
    kalman1_init(&state1, data[1], 5e2);
    // 2 dimension
    init_x[0] = data[0];
    init_x[1] = data[1] - data[0];
    kalman2_init(&state2, init_x, init_p);
    printf("%d %d %d\n", data, data_len-2, data_len-2);
    for (i=2; i<data_len; i++) {  // Filter start from 2
        printf("%.2f", data[i]);      // Original data
        /* 1 dimension */
        out1 = kalman1_filter(&state1, data[i]);
        printf(" %.2f", out1);  // Filter result
        /* 2 dimension */
        out2 = kalman2_filter(&state2, data[i]);
        printf(" %.2f\n", out2);  // Filter result
    }

    data = x2;
    data_len = sizeof(x2)/sizeof(float);
    // 1 dimension
    kalman1_init(&state1, data[1], 5e2);
    // 2 dimension
    init_x[0] = data[0];
    init_x[1] = data[1] - data[0];
    kalman2_init(&state2, init_x, init_p);
    printf("%d %d %d\n", data, data_len-2, data_len-2);
    for (i=2; i<data_len; i++) {  // Filter start from 2
        printf("%.2f", data[i]);      // Original data
        /* 1 dimension */
        out1 = kalman1_filter(&state1, data[i]);
        printf(" %.2f", out1);  // Filter result
        /* 2 dimension */
        out2 = kalman2_filter(&state2, data[i]);
        printf(" %.2f\n", out2);  // Filter result
    }

    return 0;
}
#endif

