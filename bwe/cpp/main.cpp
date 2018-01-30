
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <deque>
#include <utility>
#include <vector>
#include <math.h>

#include "kalman_filter.h" 
#include "xcmdline.h"

#define dbgv(...) do{  printf("<main>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{  printf("<main>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{  printf("<main>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)

template<class VAL_TYPE> 
class IEstimator{
public:
    virtual ~IEstimator(){};
    virtual bool push(int64_t now_ms, VAL_TYPE v) = 0;
    virtual VAL_TYPE get_aver() = 0;
    virtual VAL_TYPE get_var() = 0;
    virtual VAL_TYPE get_sign_var() = 0;
    virtual VAL_TYPE get_max_diff() = 0;
    virtual VAL_TYPE get_last_diff() = 0;
    virtual VAL_TYPE get_last_max_diff() = 0;
};

template<class VAL_TYPE> 
class AvrEstimator{
public:
    AvrEstimator(size_t n)
    : n_(n)
    , valid_(false)
    , average_(0)
    , var_(1)
    , sign_var_(1)
    , max_diff_(0)
    , last_diff_(0)
    , last_max_diff_(0)

    , val_sum_(0)
    , squ_sum_(0)
    , min_(0){
    }

    virtual bool push(int64_t now_ms, VAL_TYPE v){
        history_.push_back(std::make_pair(now_ms, v));
        last_diff_ = v - history_.back().second;
        val_sum_ += v;
        if(!valid_){
            if(history_.size() == 1) min_ = v;
            last_max_diff_ = v - min_;

            if(history_.size() == n_){
                average_ = val_sum_ / n_;
                this->make_var();
                valid_ = true;
            }
            
        }else{
            VAL_TYPE old = history_.front().second;
            history_.pop_front();
            val_sum_ -= old;
            average_ = val_sum_ / n_;
            this->make_var();
            last_max_diff_ = v - min_;
        }
        return valid_;
    }

    virtual VAL_TYPE get_aver(){
        return average_;
    }
    virtual VAL_TYPE get_var(){
        return var_;
    }
    virtual VAL_TYPE get_sign_var(){
        return sign_var_;
    }
    virtual VAL_TYPE get_max_diff(){
        return max_diff_;
    }
    virtual VAL_TYPE get_last_diff(){
        return last_diff_;
    }
    virtual VAL_TYPE get_last_max_diff(){
        return last_max_diff_;
    }
    
protected:
    void make_var(){
        
        squ_sum_ = 0;
        sign_var_ = 0;
        VAL_TYPE  max;
        int n = 0;
        for(auto o : history_){
            VAL_TYPE e = o.second-average_;
            squ_sum_ += e * e;
            sign_var_ += fabs(e);
            if(n == 0){
                min_ = max = o.second;
            }else{
                if(o.second < min_){
                    min_ = o.second;
                }
                if(o.second > max){
                    max = o.second;
                }
            }
            n++;
        }
        var_ = squ_sum_ / n_;
        // var_ = sqrt(var_);
        sign_var_ = sign_var_ /n;
        max_diff_ = max - min_;

    }
    
    size_t n_;
    bool valid_;
    VAL_TYPE average_;
    VAL_TYPE var_;
    VAL_TYPE sign_var_;
    VAL_TYPE max_diff_;
    VAL_TYPE last_diff_;
    VAL_TYPE last_max_diff_;

    VAL_TYPE val_sum_;
    VAL_TYPE squ_sum_;
    VAL_TYPE min_;
    
    std::deque<std::pair<int64_t, VAL_TYPE> > history_;
};


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


// app implementation

enum XOPTS {
    XOPT_HELP = 0,
    XOPT_INPUT ,
    XOPT_OUTPUT ,
    XOPT_VECTOR_INDEX ,
    XOPT_CQ ,
    XOPT_CR ,
    XOPT_MAX
};

// TODO: add arg of skip first column, etc
static 
const struct xcmd_option * get_app_options(){

    static struct xcmd_option app_options[XOPT_MAX+1] = {0};

    if(!app_options[0].opt.name){
        xcmdline_init_options(app_options, sizeof(app_options)/sizeof(app_options[0]));

        app_options[XOPT_HELP] = (struct xcmd_option){
            .typ = XOPTTYPE_INT,
            .mandatory = 0,
            .opt = { "help",   no_argument,  NULL, 'h' },
            .short_desc = "", 
            .long_desc = "print this message",
            .def_val = {.intval = 0, .raw = NULL},
            };

        app_options[XOPT_INPUT] = (struct xcmd_option){
            .typ = XOPTTYPE_STR,
            .mandatory = 0,
            .opt = { "input",  required_argument,  NULL, 'i' },
            .short_desc = "<input-data-file>", 
            .long_desc = "the first column must be timestamp or seq;",
            .def_val = {.strval = "in.txt", .raw = "in.txt"}
            };

        app_options[XOPT_OUTPUT] = (struct xcmd_option){
            .typ = XOPTTYPE_STR,
            .mandatory = 0,
            .opt = { "output", required_argument,  NULL, 'o' },
            .short_desc = "<output-data-file>", 
            .long_desc = "the first column is same with input-data-file first one, the second is output estimation;",
            .def_val = {.strval = "out.txt", .raw = "out.txt"}
            };

        app_options[XOPT_VECTOR_INDEX] = (struct xcmd_option){
            .typ = XOPTTYPE_INT,
            .mandatory = 0,
            .opt = { "vector-col", required_argument,  NULL, 'c' },
            .short_desc = "<vector-column>", 
            .long_desc = "column index for vector in input-data-file, 0 for first column, 1 for second column;",
            .def_val = {.intval = 1, .raw = "1"}
            };

        app_options[XOPT_CQ] = (struct xcmd_option){
            .typ = XOPTTYPE_DOUBLE,
            .mandatory = 0,
            .opt = { "noise-covar", required_argument,  NULL, 'q' },
            .short_desc = "<noise-covariance>", 
            .long_desc = "",
            .def_val = {.doubleval = 1e-8, .raw = "1e-8"}
            };

        app_options[XOPT_CR] = (struct xcmd_option){
            .typ = XOPTTYPE_DOUBLE,
            .mandatory = 0,
            .opt = { "measure-covar", required_argument,  NULL, 'r' },
            .short_desc = "<measure-covariance>", 
            .long_desc = "",
            .def_val = {.doubleval = 1e-6, .raw = "1e-6"}
            };
    }

    return app_options;
};



int main(int argc, char ** argv){
    int ret = 0;

    xoptval configs[XOPT_MAX];
    ret = xcmdline_parse(argc, argv, get_app_options(), XOPT_MAX, configs, NULL);
    if(ret || configs[XOPT_HELP].intval){
        const char * usage = xcmdline_get_usage(argc, argv, get_app_options(), XOPT_MAX);
        fprintf(stderr, "%s", usage);
        return ret;
    }
    const char * str = xcmdline_get_config_string(get_app_options(), XOPT_MAX, configs);
    fprintf(stderr, "%s", str);

    const char * input_filename = configs[XOPT_INPUT].strval;
    const char * output_filename = configs[XOPT_OUTPUT].strval;
    int vector_index = configs[XOPT_VECTOR_INDEX].intval;
    kalman_data_t cQ = configs[XOPT_CQ].doubleval;
    kalman_data_t cR = configs[XOPT_CR].doubleval;
    // kalman_data_t cQ = 1e-8, cR = 1e-6;
    // kalman_data_t cQ = 1e-4, cR = 8e-4;


    array2d datastor;
    array2d *data = &datastor;
    ret = load_txt(input_filename, data);
    if(ret < 0){
        return -1;    
    }
    dbgi("loaded input: [%s]", input_filename); 
    dbgi("rows=%d, cols=%d", data->rows, data->cols); 


    kalman1_state * states1 = NULL;
    kalman2_state * states2 = NULL;
    AvrEstimator<double> * estor1 = new AvrEstimator<double>(5);
    AvrEstimator<double> * estor2 = new AvrEstimator<double>(5);
    FILE * fp1 = NULL;
    ret = -1;
    do{
        if(data->cols <= 1){
            dbge("cols <= 1");
            ret = -1;
            break;
        }
        if(data->cols <= vector_index){
            dbge("vector column must < %d", data->cols);
            ret = -1;
            break;
        }

        fp1 = fopen(output_filename, "wb");
        if(!fp1){
            dbge("fail to write open: [%s]", output_filename);
            ret = -1;
            break;
        }
        dbgi("opened output: [%s]", output_filename); 

        int num_measures = data->cols;
        states1 = (kalman1_state *)malloc( num_measures * sizeof(kalman1_state) );
        states2 = (kalman2_state *)malloc( num_measures * sizeof(kalman2_state) );
    
        int index = vector_index;
        // init kalman by first 2 rows of data
        kalman_data_t z0 = data->at(0, index);
        kalman_data_t z1 = data->at(1, index);
        
        // kalman1_init(&states1[index], z0, 5e2);
        kalman1_init(&states1[index], 0, 1, cQ, cR);

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
            estor1->push(0, z);
            estor2->push(0, z);
            // dumplen = sprintf(dumpbuf+0, "%.2f %.6f %.6f %.6f\n",t, z, out1, out2);
            dumplen = sprintf(dumpbuf+0, "%.2f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",t, z, out1
                , estor1->get_aver()
                , estor1->get_var()
                , estor1->get_sign_var()
                , estor2->get_max_diff()
                , estor2->get_last_max_diff()
                , estor2->get_last_max_diff()
                );
            fwrite(dumpbuf, dumplen, 1, fp1);
        }


        // other rows
        for(int row = 2; row < data->rows; row++){
            kalman_data_t t = data->at(row, 0);
            kalman_data_t z = data->at(row, index);
            kalman_data_t out1 = kalman1_filter(&states1[index], z);
            kalman_data_t out2 = kalman2_filter(&states2[index], z);
            estor1->push(0, z);
            estor2->push(0, z);
            dumplen = sprintf(dumpbuf+0, "%.2f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",t, z, out1
                , estor1->get_aver()
                , estor1->get_var()
                , estor1->get_sign_var()
                , estor2->get_max_diff()
                , estor2->get_last_max_diff()
                , estor2->get_last_max_diff()
                );
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



