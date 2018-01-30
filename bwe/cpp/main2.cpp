
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

#include "kalman_filter.h" 
#include "xcmdline.h"

#define dbgv(...) do{  printf("<main>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{  printf("<main>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{  printf("<main>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)


typedef struct xdata_item{
    int row;
    kalman_data_t value;
}xdata_item;

typedef struct xvector{
    char name[128];
    int num_rows;
    xdata_item * data; 
}xvector;

typedef struct xvector_array{
    xvector * vectors;
    int num_vectors;
}xvector_array;

void xvector_free(xvector * vector){
    if(vector->data){
        free(vector->data);
        vector->data  = NULL;
    }
}

void xvector_array_free(xvector_array * arr){
    if(arr->vectors){
        for(int i = 0; i < arr->num_vectors; i++){
            xvector_free(&arr->vectors[i]);
        }
        free(arr->vectors);
        arr->vectors = NULL;
    }
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


static
int load_txt(const char * filename, xvector_array *arr){
    FILE * fp = NULL;
    // kalman_data_t * data = NULL;
    int ret = -1;
    char buf[1002];
    int num_lines = 0;
    int num_cols = 0;
    do{
        memset(arr, 0, sizeof(xvector_array));

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
    }
    return ret;
}

#define MIN_VALUE_THRESHOLD 6.0 // 12.5
#define RESTART_THRESHOLD  -10
#define MAX_SLOPE_THRESHOLD (10.0/1000.0)
#define SLOPE_LIMIT_LOW (-2.0/1000.0)
#define MAX_SLOPE_INTERVAL 1500
#define SLOPE_RANGE_TIME_INTERVAL 500
#define SLOPE_CHECK_INTERVAL 3500
#define MAX_SLOPE_HISTORY_INTERVAL (SLOPE_CHECK_INTERVAL+SLOPE_RANGE_TIME_INTERVAL+SLOPE_RANGE_TIME_INTERVAL/2)


struct XRange{
    XRange(double v):min_(v), max_(v){}
    double min_;
    double max_;
};

class OverThresholdEstimator{
protected:
    int64_t start_time_ = -1;
    double threshold_ = MIN_VALUE_THRESHOLD;
    int64_t last_reset_time_ = -1;
    int64_t last_time_ = -1;
    double last_value_ = 0;
    double value_est_ = 0;
    double slope_est_ = 0;
    double max_slope_ = 0;
    double min_slope_ = 0;
    
    std::deque<std::pair<int64_t, XRange> > slop_range_history_;
    std::deque<std::pair<int64_t, double> > value_history_;

public:
    OverThresholdEstimator(){}
    virtual ~OverThresholdEstimator(){}

    void reset(int64_t time_ms, double z){
            last_reset_time_ = time_ms;
            value_est_ = z;
            max_slope_ = 0;
            min_slope_ = 0;
            slope_est_ = 0;
            slop_range_history_.clear();
            slop_range_history_.push_back(std::make_pair(time_ms, XRange(0)  ) );
            value_history_.clear();
            value_history_.push_back( std::make_pair(time_ms, z ) );
    }

    bool checkCondition(int64_t time_ms){
            int64_t interval = time_ms - slop_range_history_.front().first;
            if(interval >= SLOPE_CHECK_INTERVAL){
                if( (max_slope_ - min_slope_) < MAX_SLOPE_THRESHOLD && min_slope_ > SLOPE_LIMIT_LOW){
                    return true;
                }
            }
            return false;
    }

    void inputValue(int64_t now_ms, double z){
        last_time_ = now_ms;
        last_value_ = z;
        
        if(start_time_ < 0){
            start_time_ = now_ms;
            reset(0, z);
            return ;
            
        }

        int64_t time_ms = now_ms - start_time_;
        double diff = z - value_est_;
        if(diff < RESTART_THRESHOLD){
            if(checkCondition(time_ms)){
                threshold_ = std::max( (value_est_+2), MIN_VALUE_THRESHOLD);
            }
            reset(time_ms, z);
            return ;
        }


        value_history_.push_back( std::make_pair(time_ms, z ) );
        value_est_ = 0.85*value_est_ + 0.15*z;
        int64_t interval = time_ms - value_history_.front().first;
        
        if(interval > MAX_SLOPE_INTERVAL/2){
            double delta_val = z - value_history_.front().second;
            double slope = delta_val / interval;

            if( ( time_ms - last_reset_time_) < MAX_SLOPE_INTERVAL){
                slope_est_ = slope;
            }else{
                slope_est_ = 0.5*slope_est_ + 0.5*slope; 
            }
            

            // remove expire values
            while(interval > MAX_SLOPE_INTERVAL){
                value_history_.pop_front();
                interval = time_ms - value_history_.front().first;
            }

            // check max & min slope
            if(slope_est_ > slop_range_history_.back().second.max_){
                slop_range_history_.back().second.max_ = slope_est_;
                if(slope_est_ > max_slope_){
                    max_slope_ = slope_est_;
                }
            }
            if(slope_est_ < slop_range_history_.back().second.min_){
                slop_range_history_.back().second.min_ = slope_est_;
                if(slope_est_ < min_slope_){
                    min_slope_ = slope_est_;
                }
            }

            if(value_est_ > threshold_){
                if(checkCondition(time_ms)){
                    threshold_ = value_est_+2;
                }
            }
        }


        

        if( (time_ms - slop_range_history_.back().first) >= SLOPE_RANGE_TIME_INTERVAL ){
            slop_range_history_.push_back(std::make_pair(time_ms, XRange(slope_est_)  ) );
            interval = time_ms - slop_range_history_.front().first;
            if(interval > MAX_SLOPE_HISTORY_INTERVAL){
                // dbgi("interval =%lld", interval);
                do{
                    slop_range_history_.pop_front();
                    interval = time_ms - slop_range_history_.front().first;
                }while(interval > MAX_SLOPE_HISTORY_INTERVAL);
                // dbgi("interval2 =%lld", interval);

                min_slope_ = slop_range_history_.front().second.min_;
                max_slope_ = slop_range_history_.front().second.max_;
                for(auto o : slop_range_history_){
                    if(o.second.min_ < min_slope_){
                        min_slope_ = o.second.min_;
                    }
                    if(o.second.max_ > max_slope_){
                        max_slope_ = o.second.max_;
                    }
                }
                // dbgi("change slope: n %2lu, range %f(%f - %f), duration %lld(%lld - %lld)", slop_range_history_.size()
                //     , max_slope_-min_slope_, max_slope_, min_slope_
                //     , time_ms - slop_range_history_.front().first, time_ms, slop_range_history_.front().first);
            }
            
        }
    }

    int dumpHeader(char * buf){
        return sprintf(buf, "Timestamp, Value, ValueEst, Threshold, SlopeEst, SlopeRange");
    }
    int dump(char * buf){
        return sprintf(buf, "%lld, %f, %f, %f, %f, %f", last_time_, last_value_, value_est_, threshold_, slope_est_, max_slope_-min_slope_);
    }
};


// app implementation

enum XOPTS {
    XOPT_HELP = 0,
    XOPT_INPUT ,
    XOPT_OUTPUT ,
    XOPT_VECTOR_INDEX ,
    XOPT_VECTOR_NAME ,
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

        app_options[XOPT_VECTOR_NAME] = (struct xcmd_option){
            .typ = XOPTTYPE_STR,
            .mandatory = 0,
            .opt = { "vector-name", required_argument,  NULL, 'm' },
            .short_desc = "<vector-name>", 
            .long_desc = "vector name in input-data-file;",
            .def_val = {.strval = "-", .raw = "-"}
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
    const char * vector_name = configs[XOPT_VECTOR_NAME].strval;

    xvector_array arrstor;
    xvector_array *arr = &arrstor;
    ret = load_txt(input_filename, arr);
    if(ret < 0){
        return -1;    
    }
    dbgi("loaded input: [%s]", input_filename); 
    dbgi("vectors=%d, rows=%d",  arr->num_vectors, arr->vectors[0].num_rows); 


    FILE * fp1 = NULL;
    ret = -1;
    do{
        if(*vector_name != '-'){
            vector_index = -1;
            for(int i = 0; i < arr->num_vectors; i++){
                // dbgi("vectors[%d]=[%s]", i, arr->vectors[i].name);
                if(strcmp(arr->vectors[i].name, vector_name) == 0 ){
                    vector_index = i;
                    break;
                }
            }
            if(vector_index < 0){
                dbge("can't find vector [%s]", vector_name);
                ret = -1;
                break;
            }
            dbgi("found vector [%s] at index %d", vector_name, vector_index);
        }

        if(arr->num_vectors <= 1){
            dbge("vectors <= 1");
            ret = -1;
            break;
        }
        
        if(arr->num_vectors <= vector_index){
            dbge("vector column must < %d", arr->num_vectors);
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

        char dumpbuf[256], dumpbuf2[256];
        int dumplen = 0, dumplen2 = 0;

        OverThresholdEstimator estimator_stor;
        OverThresholdEstimator * estimator = &estimator_stor;
        dumplen = estimator->dumpHeader(dumpbuf);
        dumpbuf[dumplen] = '\n'; dumpbuf[dumplen+1] = '\0'; ++dumplen;
        fwrite(dumpbuf, dumplen, 1, fp1);

        xvector * vector = &arr->vectors[vector_index];
        xvector * timev = &arr->vectors[0];
        int num_datas = 0;
        for(int i = 0; i < vector->num_rows; i++){
            if(vector->data[i].row < 0 || timev->data[i].row < 0){
                // dbgi("skip row %d", i);
                continue;
            }
            // dbgi("process row %d", i);
            estimator->inputValue(timev->data[i].value, vector->data[i].value);
            dumplen = estimator->dump(dumpbuf);
            dumpbuf[dumplen] = '\n'; dumpbuf[dumplen+1] = '\0'; ++dumplen;
            fwrite(dumpbuf, dumplen, 1, fp1);
            ++num_datas;
        }
        dbgi("process data num %d", num_datas);

        // int num_measures = data->cols;
        // states1 = (kalman1_state *)malloc( num_measures * sizeof(kalman1_state) );
        // states2 = (kalman2_state *)malloc( num_measures * sizeof(kalman2_state) );
    
        // int index = vector_index;
        // // init kalman by first 2 rows of data
        // kalman_data_t z0 = data->at(0, index);
        // kalman_data_t z1 = data->at(1, index);
        
        // // kalman1_init(&states1[index], z0, 5e2);
        // kalman1_init(&states1[index], 0, 1, cQ, cR);

        // kalman_data_t init_x[2] = {z0, z1-z0};
        // kalman_data_t init_p[2][2] = {{10e-6,0}, {0,10e-6}};
        // kalman2_init(&states2[index], init_x, init_p);


        // // first 2 rows 
        // char dumpbuf[256], dumpbuf2[256];
        // int dumplen = 0, dumplen2 = 0;
        // dumplen = 0;
        // for(int row = 0; row < 2; row++){
        //     kalman_data_t t = data->at(row, 0);
        //     kalman_data_t z = data->at(row, index);
        //     kalman_data_t out1 = kalman1_filter(&states1[index], z);
        //     kalman_data_t out2 = z;
        //     estor1->push(0, z);
        //     estor2->push(0, z);
        //     // dumplen = sprintf(dumpbuf+0, "%.2f %.6f %.6f %.6f\n",t, z, out1, out2);
        //     dumplen = sprintf(dumpbuf+0, "%.2f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",t, z, out1
        //         , estor1->get_aver()
        //         , estor1->get_var()
        //         , estor1->get_sign_var()
        //         , estor2->get_max_diff()
        //         , estor2->get_last_max_diff()
        //         , estor2->get_last_max_diff()
        //         );
        //     fwrite(dumpbuf, dumplen, 1, fp1);
        // }


        // // other rows
        // for(int row = 2; row < data->rows; row++){
        //     kalman_data_t t = data->at(row, 0);
        //     kalman_data_t z = data->at(row, index);
        //     kalman_data_t out1 = kalman1_filter(&states1[index], z);
        //     kalman_data_t out2 = kalman2_filter(&states2[index], z);
        //     estor1->push(0, z);
        //     estor2->push(0, z);
        //     dumplen = sprintf(dumpbuf+0, "%.2f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",t, z, out1
        //         , estor1->get_aver()
        //         , estor1->get_var()
        //         , estor1->get_sign_var()
        //         , estor2->get_max_diff()
        //         , estor2->get_last_max_diff()
        //         , estor2->get_last_max_diff()
        //         );
        //     fwrite(dumpbuf, dumplen, 1, fp1);
        // }


        ret = 0;
        dbgi("done");
    }while(0);

    if(fp1){
        fclose(fp1);
        fp1 = NULL;
    }

    // if(states1){
    //     free(states1);
    //     states1 = NULL;
    // }

    // if(states2){
    //     free(states2);
    //     states2 = NULL;
    // }

    if(arr){
        xvector_array_free(arr);
    }

    return 0;
}



