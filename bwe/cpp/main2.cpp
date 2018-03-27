
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <deque>
#include <utility>
#include <vector>
#include <math.h>
#include <cmath>
#include <algorithm> // std::max

#include "kalman_filter.h" 
#include "xcmdline.h"
#include "xdatafile.h"
#include "inter_arrival.h"
#include "overuse_estimator.h"
#include "overuse_detector.h"

#define dbgv(...) do{  printf("<main>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{  printf("<main>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{  printf("<main>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)

using namespace webrtc;

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
    void setValue(double v){min_=v; max_=v;};
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

class SampleRateEstimator{
protected:
    const int64_t maxInterval_ = 1000;
    int samplerate_ = 90000;
    int64_t start_time_ = -1;
    uint32_t lastZ_ = 0;
    int64_t lastExtz_ = 0;
    std::deque<std::pair<int64_t, int64_t> > value_history_;

public:
    void reset(int64_t time_ms, uint32_t z){

            value_history_.clear();
            value_history_.push_back( std::make_pair(time_ms, z ) );
    }

    void input(int64_t now_ms, uint32_t zzz){
        if(start_time_ < 0){
            start_time_ = now_ms;
            lastZ_ = zzz;
            lastExtz_ = zzz;
            reset(0, zzz);
            return ;
        }
        int64_t time_ms = now_ms - start_time_;

        uint32_t d1 = zzz - lastZ_;
        uint32_t d2 = lastZ_ - zzz;
        uint32_t distance = std::min(d1, d2);
        // if(distance > (30 * 90000)){
        if(distance > (20000)){
            reset(time_ms, zzz);
        }
        if(zzz < lastZ_){
            // disorder 
            return;
        }
        lastZ_ = zzz;
        lastExtz_ += d1;

        
        value_history_.push_back( std::make_pair(time_ms, lastExtz_ ) );
        int64_t interval = time_ms - value_history_.front().first;
        if( interval >= maxInterval_ ){
            int64_t delta_val = lastExtz_ - value_history_.front().second;
            int Hz = delta_val*1000/interval;
            samplerate_ = 0.99 * samplerate_ + 0.01 * Hz;

            do{
                value_history_.pop_front();
                interval = time_ms - value_history_.front().first;
            }while(interval >= maxInterval_);
        }
    }

    int getLatest(){
        return samplerate_;
    }
};

struct BweSingleStreamDetector {
  explicit BweSingleStreamDetector(int64_t last_packet_time_ms,
                    const OverUseDetectorOptions& options,
                    bool enable_burst_grouping)
      : last_packet_time_ms(last_packet_time_ms),
        inter_arrival(90 * kTimestampGroupLengthMs,
                      kTimestampToMs,
                      enable_burst_grouping),
        estimator(options),
        detector(options),
        num_packets(0),
        last_rtp_timestamp(0),
        last_timestamp_delta_ms(0),
        last_time_delta(0){
//            dumpor = create_xdump();
        }
    ~BweSingleStreamDetector(){
    }


  int64_t last_packet_time_ms;
  InterArrival inter_arrival;
  OveruseEstimator estimator;
  OveruseDetector detector;
  SampleRateEstimator HzEstimator;
    int64_t num_packets ;
    uint32_t last_rtp_timestamp;
    double last_timestamp_delta_ms;
    int64_t last_time_delta;
};

// app implementation

enum XOPTS {
    XOPT_HELP = 0,
    XOPT_INPUT ,
    XOPT_OUTPUT ,
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

    }

    return app_options;
};


#define DEFINE_FIND_VECTOR(v, name, xarr, idx)   \
    xvector * v = xvector_array_find(xarr, name, &(idx));\
    if(!v){ \
        dbge("fail to found vector: [%s]", name); \
        ret = -1; \
        break;    \
    }\
    dbgi("found vector: [%d]-[%s]", idx, name);



int main_single(int argc, char ** argv){
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
    const int main_ssrc = 1843363638;
    bool no_rtp_ts = false;

    xvector_array *arr = load_txt(input_filename); 
    if(!arr){
        dbge("fail to load [%s]", input_filename);
        return -1;    
    }
    dbgi("loaded input: [%s]", input_filename); 
    dbgi("vectors=%d, rows=%d",  arr->num_vectors, arr->vectors[0].num_rows); 
    dbgi("main_ssrc=%d", main_ssrc);

    BweSingleStreamDetector * estimator = NULL;
    bool enable_burst_grouping = true;
    FILE * fp1 = NULL;
    ret = -1;
    do{
        if(arr->num_vectors <= 1){
            dbge("vectors <= 1");
            ret = -1;
            break;
        }

        int vector_index = -1;
        DEFINE_FIND_VECTOR(rtp_ts_v, "RtpTimestamp", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_ssrc_v, "RtpSsrc", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_arrt_v, "RtpArriTime", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_plsize_v, "RtpPlsize", arr, vector_index);                                 
        DEFINE_FIND_VECTOR(time_delta_v, "TimeDelta", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_time_delta_v, "RtpTimeDelta", arr, vector_index);
        DEFINE_FIND_VECTOR(size_delta_v, "SizeDelta", arr, vector_index);
        DEFINE_FIND_VECTOR(last_t_v, "LastT", arr, vector_index);


        fp1 = fopen(output_filename, "wb");
        if(!fp1){
            dbge("fail to write open: [%s]", output_filename);
            ret = -1;
            break;
        }
        dbgi("opened output: [%s]", output_filename); 

        char dumpbuf[512];
        int dumplen = 0;

        
        OverThresholdEstimator threshold_estimator_stor;
        OverThresholdEstimator * threshold_estimator = NULL;
        xvector * vector = NULL; // last_t_v
        if(vector){
            threshold_estimator = &threshold_estimator_stor;
            dumplen = threshold_estimator->dumpHeader(dumpbuf);
            dumpbuf[dumplen] = '\n'; dumpbuf[dumplen+1] = '\0'; ++dumplen;
            fwrite(dumpbuf, dumplen, 1, fp1);
        }

        dumplen = sprintf(dumpbuf, "Timestamp, LastT, Threshold, EstT, TimeDelta, RtpTimeDelta, SizeDelta, NumDeltas, Hz, TTSDelta, TTSDeltaEst, LastSlope\n");
        fwrite(dumpbuf, dumplen, 1, fp1);



        int64_t now_ms = 0;

        uint32_t timestamp_delta = 0;
        int64_t time_delta = 0;
        int size_delta = 0;
        double timestamp_delta_ms = 0;
        double t_ts_delta = 0;

        xvector * timev = &arr->vectors[0];
        int num_datas = 0;
        for(int i = 0; i < timev->num_rows; i++){
            if(timev->data[i].row < 0){
                // dbgi("skip row %d", i);
                continue;
            }

            dbgi("process row %d", i);
            now_ms = timev->data[i].value;
            
            if(vector && vector->data[i].row >= 0 ){
                threshold_estimator->inputValue(now_ms, vector->data[i].value);
                dumplen = threshold_estimator->dump(dumpbuf);
                dumpbuf[dumplen] = '\n'; dumpbuf[dumplen+1] = '\0'; ++dumplen;
                fwrite(dumpbuf, dumplen, 1, fp1);
                ++num_datas;
            }

            if(rtp_ts_v->data[i].row >= 0){
                uint32_t ssrc = rtp_ssrc_v->data[i].value;
                if(ssrc == main_ssrc){
                    if(!estimator){
                        estimator = new BweSingleStreamDetector(now_ms, OverUseDetectorOptions(), enable_burst_grouping);
                    }
                    estimator->last_packet_time_ms = now_ms;
                    estimator->num_packets++;

                    
                    uint32_t rtp_timestamp = rtp_ts_v->data[i].value;
                    int64_t arrival_time_ms = rtp_arrt_v->data[i].value;
                    size_t payload_size = rtp_plsize_v->data[i].value;

                    estimator->HzEstimator.input(now_ms, rtp_timestamp);

                    const BandwidthUsage prior_state = estimator->detector.State();
                    if (estimator->inter_arrival.ComputeDeltas(rtp_timestamp, arrival_time_ms,
                                             payload_size, &timestamp_delta,
                                             &time_delta, &size_delta)) {
                        timestamp_delta_ms = timestamp_delta * kTimestampToMs;
                        
                        // int Hz = estimator->HzEstimator.getLatest();
                        // timestamp_delta_ms = timestamp_delta * 1000/Hz;
                        // dbgi("Hz=%d", Hz);
                        
                        estimator->estimator.Update(time_delta, timestamp_delta_ms, size_delta,
                                                    estimator->detector.State());
                        estimator->detector.Detect(estimator->estimator.offset(),
                                                   timestamp_delta_ms,
                                                   estimator->estimator.num_of_deltas(), now_ms);

                    }
                }
            }

            if(time_delta_v->data[i].row >= 0){
                if(!estimator){
                    estimator = new BweSingleStreamDetector(now_ms, OverUseDetectorOptions(), enable_burst_grouping);
                    if(rtp_ts_v->data[i].row < 0){
                        no_rtp_ts = true;    
                    }
                }

                if(no_rtp_ts){
                    timestamp_delta_ms = rtp_time_delta_v->data[i].value;
                    time_delta = time_delta_v->data[i].value;
                    size_delta = size_delta_v->data[i].value;
                    estimator->estimator.Update(time_delta, timestamp_delta_ms, size_delta,
                                                estimator->detector.State());
                    estimator->detector.Detect(estimator->estimator.offset(),
                                               timestamp_delta_ms,
                                               estimator->estimator.num_of_deltas(), now_ms);                    
                }

                int num_of_deltas = estimator->estimator.num_of_deltas();
                double offset = estimator->estimator.offset();
                const double T = std::min(num_of_deltas, 60) * offset;



                // double diffT;
                // diffT = timestamp_delta_ms - rtp_time_delta_v->data[i].value;
                // if(fabs(diffT) >1e-6){
                //     dbge("timestamp_delta_ms: diffT = %f !!!!!!", diffT);
                //     ret = -1;
                //     break;
                // }

                // diffT = size_delta - size_delta_v->data[i].value;
                // if(fabs(diffT) >1e-6){
                //     dbge("size_delta: diffT = %f (%d, %f) !!!!!!", diffT, size_delta, size_delta_v->data[i].value);
                //     ret = -1;
                //     break;
                // }

                // diffT = time_delta - time_delta_v->data[i].value;
                // if(fabs(diffT) >1e-6){
                //     dbge("time_delta: diffT = %f !!!!!!", diffT);
                //     ret = -1;
                //     break;
                // }

                // diffT = T - last_t_v->data[i].value;
                // if(fabs(diffT) > 1e-6){
                //     dbge("lastT diffT = %f !!!!!!", diffT);
                //     ret = -1;
                //     break;
                // }

                double tt = time_delta_v->data[i].value - rtp_time_delta_v->data[i].value;
                t_ts_delta = 0.99*t_ts_delta + 0.01*tt;

                dumplen = sprintf(dumpbuf, "%lld, %f, %f, %f, %lld, %f, %d, %d, %d, %f, %f, %f\n", now_ms
                    , last_t_v->data[i].value
                    , estimator->detector.Threshold()
                    , T
                    , time_delta
                    , timestamp_delta_ms
                    , size_delta
                    , num_of_deltas
                    , estimator->HzEstimator.getLatest()
                    , tt, t_ts_delta
                    , estimator->estimator.slope());
                fwrite(dumpbuf, dumplen, 1, fp1);



                ++num_datas;
                ret = 0;
            }
            
        }
        if(ret) break;

        dbgi("process data num %d", num_datas);


        ret = 0;
        dbgi("done");
    }while(0);

    if(estimator){
        delete estimator;
        estimator = NULL;
    }

    if(fp1){
        fclose(fp1);
        fp1 = NULL;
    }


    if(arr){
        xvector_array_free(arr);
        arr = NULL;
    }

    return 0;
}


// class IntegralWindow{
// protected:
//     int64_t lastTime_ = -1;
//     double lastValue_ = 0;
//     int64_t winSize_ = 0;
//     double sum_ = 0;
//     int64_t duration_ = 0;
//     std::deque<std::pair<int64_t, double> > history_;
// public:
//     IntegralWindow(int64_t winSize):winSize_(winSize){

//     }
//     bool update(int64_t now_ms, double value){
//         if(lastTime_ < 0){
//             lastTime_ = now_ms;
//             lastValue_ = value;
//             return false;
//         }
//         int64_t dt = now_ms - lastTime_;
//         double dsum = lastValue_ * dt;
//         lastTime_ = now_ms;
//         lastValue_ = value;

//         sum_ += dsum;
//         duration_ += dt;

//         history_.push_back(std::make_pair(dt, dsum ) );

//         if(duration_ < winSize_){
//             return false;
//         }
//         return true;
//     }
//     double getSum(bool autoPop=true){
//         double sum = sum_;
//         if(autoPop){
//             while(duration_ >= winSize_){
//                 duration_ -= history_.front().first;
//                 sum_ -= history_.front().second;
//                 history_.pop_front();
//             }
//         }
//         return sum;
//     }
//     double duration(){
//         return duration_;
//     }
// };

// class IntegralWindow{
// protected:
//     int64_t lastTime_ = -1;
//     double lastValue_ = 0;
//     int64_t winSize_ = 0;
//     double sum_ = 0;
//     int64_t duration_ = 0;
//     std::deque<std::pair<int64_t, double> > history_;
// public:
//     IntegralWindow(int64_t winSize=1000):winSize_(winSize){
        
//     }
//     bool update(int64_t now_ms, double value){
//         if(lastTime_ < 0){
//             lastTime_ = now_ms;
//             lastValue_ = value;
//             return false;
//         }
//         int64_t dt = now_ms - lastTime_;
//         double dsum = lastValue_ * dt;
//         lastTime_ = now_ms;
//         lastValue_ = value;
        
//         sum_ += dsum;
//         duration_ += dt;
        
//         history_.push_back(std::make_pair(dt, dsum ) );
        
//         if(duration_ < winSize_){
//             return false;
//         }
//         return true;
//     }
//     double getSum(bool autoPop=true){
//         if(autoPop){
//             while(duration_ >= winSize_){
//                 duration_ -= history_.front().first;
//                 sum_ -= history_.front().second;
//                 history_.pop_front();
//             }
//         }
//         double sum = sum_;
//         return sum;
//     }
//     double duration(){
//         return duration_;
//     }
// };

class IntegralWindow{
protected:
    int64_t winSize_;
    int64_t winError_;
    int64_t lastTime_ = -1;
    double lastValue_ = 0;
    double sum_ = 0;
    int64_t duration_ = 0;
    std::deque<std::pair<int64_t, double> > history_;
    
    void popExpire(int64_t now_ms){
        while(history_.size() > 0 && duration_ > (winSize_+winError_)) {
            duration_ -= history_.front().first;
            sum_ -= history_.front().second;
            history_.pop_front();
        }
        if(history_.size() <= 1){
            history_.clear();
            history_.push_back(std::make_pair(0, 0 ) );
            sum_ = 0;
            duration_ = 0;
        }
    }
    
public:
    IntegralWindow(int64_t winSize=1000, int64_t winError = 500):winSize_(winSize), winError_(winError){
    }
    bool update(int64_t now_ms, double value, bool autoPop=true){
        if(lastTime_ < 0){
            lastTime_ = now_ms;
            lastValue_ = value;
            return false;
        }
        int64_t dt = now_ms - lastTime_;
        double dsum = lastValue_ * dt;
        lastTime_ = now_ms;
        lastValue_ = value;
        
        sum_ += dsum;
        duration_ += dt;
        
        history_.push_back(std::make_pair(dt, dsum ) );
        if(autoPop){
            popExpire(now_ms);
        }
        
        if(duration_ < winSize_){
            return false;
        }
        return true;
    }
    bool valid(){
        return duration_ >= winSize_ && duration_ <= (winSize_+winError_);
    }
    double sum(bool autoPop=true){
        return sum_;
    }
    double getSum(bool autoPop=true){
        return this->sum(autoPop);
    }
    double duration(){
        return duration_;
    }
};




enum {
  kTimestampGroupLengthMs_abs = 5,
  kAbsSendTimeFraction = 18,
  kAbsSendTimeInterArrivalUpshift = 8,
  kInterArrivalShift = kAbsSendTimeFraction + kAbsSendTimeInterArrivalUpshift,
  kInitialProbingIntervalMs = 2000,
  kMinClusterSize = 4,
  kMaxProbePackets = 15,
  kExpectedNumberOfProbes = 3
};
static const double kTimestampToMs_abs = 1000.0 /
    static_cast<double>(1 << kInterArrivalShift);


static 
int dump_header(FILE * fp1, xvector * timev, const char * var_names[], int &first_ts_index, int &nvars){
    char dumpbuf[512];
    int dumplen = 0;

    nvars = 0;
    const char * name = var_names[nvars];
    dumplen += sprintf(dumpbuf+dumplen, "Timestamp");
    while(name){
        dumplen += sprintf(dumpbuf+dumplen, ", %s", name);
        ++nvars;
        name = var_names[nvars];
    }
    dumplen += sprintf(dumpbuf+dumplen, "\n");
    fwrite(dumpbuf, dumplen, 1, fp1);
    

    if(first_ts_index >= 0){
        for(int i = 0; i < timev->num_rows; i++){
            if(timev->data[i].row >= 0){
                first_ts_index = i;
                int64_t now_ms = timev->data[i].value;
                dumplen = 0;
                dumplen += sprintf(dumpbuf+dumplen, "%lld", now_ms);
                for(int i = 0; i < nvars; i++){
                    dumplen += sprintf(dumpbuf+dumplen, ", -");
                }
                dumplen += sprintf(dumpbuf+dumplen, "\n");
                fwrite(dumpbuf, dumplen, 1, fp1); 
                break;
            }
        }

    }
    return 0;
}


int abs_integral_process(FILE * fp1, xvector_array *arr){
    int ret = 0;
    IntegralWindow * iwin = new IntegralWindow(1000);
    do{
        int vector_index = -1;
        DEFINE_FIND_VECTOR(last_t_v, "LastT", arr, vector_index);
        DEFINE_FIND_VECTOR(t_ts_delta_v, "TTSDelta", arr, vector_index);
        xvector * timev = &arr->vectors[0];

        const char * dump_var_names[] = { "LastT", "TTSDelta", "TTSDeltaEst", "TTSIntegral", NULL };
        int i = 0;
        int nvars = 0;
        ret = dump_header(fp1, timev, dump_var_names, i, nvars);

        char dumpbuf[512];
        int dumplen = 0;
        int num_datas = 0;
        for(; i < timev->num_rows; i++){
            if(timev->data[i].row < 0){
                // dbgi("skip row %d", i);
                continue;
            }

            dbgi("process row %d", i);
            int64_t now_ms = timev->data[i].value;
            bool is_dump = false;


            if(t_ts_delta_v->data[i].row >= 0){
                iwin->update(now_ms, t_ts_delta_v->data[i].value);
            }
            double t_ts_integral = iwin->getSum();

            double t_ts_delta = t_ts_delta_v->data[i].value;
            double t_ts_delta_est = 0.99*t_ts_delta_est + 0.01*t_ts_delta;

            if(last_t_v->data[i].row >= 0){
                dumplen = sprintf(dumpbuf, "%lld, %f, %f, %f, %f\n", now_ms, last_t_v->data[i].value , t_ts_delta, t_ts_delta_est, t_ts_integral);
                fwrite(dumpbuf, dumplen, 1, fp1);
                is_dump = true;                
                ++num_datas;
            }
        }
        dbgi("process data num %d", num_datas);

    }while(0);
    if(iwin){
        delete iwin;
        iwin = NULL;
    }

    return ret;
}

class RateEstimator{
protected:
    int64_t max_window_size_ms_;
    double scale_;
    double sum_ = 0;
    std::deque<std::pair<int64_t, double> > history_;

    void eraseOld(int64_t now_ms){
        while(history_.size() > 0){
            int64_t elapse = now_ms - history_.front().first;
            if(elapse <= max_window_size_ms_){
                break;
            }
            sum_ -= history_.front().second;
            history_.pop_front();
        }
        if(history_.size() == 0){
            history_.push_back(std::make_pair(now_ms, 0));    
        }
    }
public:
    RateEstimator(int64_t max_window_size_ms, double scale)
    : max_window_size_ms_(max_window_size_ms), scale_(scale){
    }

    void update(int64_t now_ms, double value){
        if(history_.size() > 0 && history_.back().first == now_ms){
            history_.back().second += value;
        }else{
            history_.push_back(std::make_pair(now_ms, value));
        }
        sum_ += value;
        eraseOld(now_ms);
    }
    double rate(int64_t now_ms){
        eraseOld(now_ms);
        if(history_.size() == 0){
            return 0;   
        }
        int64_t elapse = now_ms - history_.front().first; 
        if(elapse < max_window_size_ms_){
            elapse = max_window_size_ms_;
        }
        double r = sum_ * scale_ / elapse;
        return r;
    }

};

int abs_verify_recvrate_process(FILE * fp1, xvector_array *arr){
    int ret = 0;
    RateEstimator * rator = new RateEstimator(1000, 8000);
    do{

        const char * dump_var_names[] = { "RecvRate", "RecvRateEst", NULL };
        xvector * timev = &arr->vectors[0];
        int i = 0;
        int nvars = 0;
        ret = dump_header(fp1, timev, dump_var_names, i, nvars);


        int vector_index = -1;
        DEFINE_FIND_VECTOR(rtp_arrt_v, "RtpArriTime", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_plsize_v, "RtpPlsize", arr, vector_index);
        DEFINE_FIND_VECTOR(recv_rate_v, "RecvRate", arr, vector_index);  
        
        char dumpbuf[512];
        int dumplen = 0;
        int num_datas = 0;
        for(; i < timev->num_rows; i++){
            if(timev->data[i].row < 0){
                // dbgi("skip row %d", i);
                continue;
            }

            dbgi("process row %d", i);
            int64_t now_ms = timev->data[i].value;

            if(rtp_arrt_v->data[i].row >= 0){
                rator->update(now_ms, rtp_plsize_v->data[i].value);
            }

            if(recv_rate_v->data[i].row >= 0){
                double raw_value = recv_rate_v->data[i].value;
                double new_value = rator->rate(now_ms);
                double diff = (new_value-raw_value);
                if(fabs(diff) > 1e-6){
                    // dbgi("row %d, raw=%f, new=%f, diff=%f", i, raw_value, new_value, diff);
                    // ret = -1;
                    // break;
                }
                dumplen = sprintf(dumpbuf, "%lld, %f, %f\n", now_ms, raw_value , new_value);
                fwrite(dumpbuf, dumplen, 1, fp1);
                ++num_datas;
            }
        }
        dbgi("process data num %d", num_datas);

    }while(0);
    if(rator){
        delete rator;
        rator = NULL;
    }

    return ret;
}



int abs_verify_offset_process(FILE * fp1, xvector_array *arr){
    int ret = 0;

    // now_ms, OverUseDetectorOptions(), enable_burst_grouping

    const OverUseDetectorOptions& options = OverUseDetectorOptions();
    OveruseEstimator estimator(options);
    OveruseDetector detector(options);
    InterArrival * inter_arrival = new InterArrival((kTimestampGroupLengthMs_abs << kInterArrivalShift) / 1000, kTimestampToMs_abs, true);
    do{

        const char * dump_var_names[] = { "LastT", "LastTEst", NULL };
        xvector * timev = &arr->vectors[0];
        int i = 0;
        int nvars = 0;
        ret = dump_header(fp1, timev, dump_var_names, i, nvars);


        int vector_index = -1;
        
        DEFINE_FIND_VECTOR(send_time24_v, "SendTime24", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_ssrc_v, "RtpSsrc", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_arrt_v, "RtpArriTime", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_plsize_v, "RtpPlsize", arr, vector_index);                                 
        DEFINE_FIND_VECTOR(time_delta_v, "TimeDelta", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_time_delta_v, "RtpTimeDelta", arr, vector_index);
        DEFINE_FIND_VECTOR(size_delta_v, "SizeDelta", arr, vector_index);
        DEFINE_FIND_VECTOR(last_t_v, "LastT", arr, vector_index);
        DEFINE_FIND_VECTOR(t_ts_delta_v, "TTSDelta", arr, vector_index);

        
        uint32_t timestamp_delta = 0;
        int64_t time_delta = 0;
        int size_delta = 0;
        double timestamp_delta_ms = 0;
        double t_ts_delta = 0;
        int num_of_deltas = 0;
        double offset = 0;

        char dumpbuf[512];
        int dumplen = 0;
        int num_datas = 0;
        for(; i < timev->num_rows; i++){
            if(timev->data[i].row < 0){
                // dbgi("skip row %d", i);
                continue;
            }

            dbgi("process row %d", i);
            int64_t now_ms = timev->data[i].value;
            if(send_time24_v->data[i].row >= 0){

                    uint32_t send_time_24bits = send_time24_v->data[i].value;
                    int64_t arrival_time_ms = rtp_arrt_v->data[i].value;
                    size_t payload_size = rtp_plsize_v->data[i].value;

                    uint32_t timestamp = send_time_24bits << kAbsSendTimeInterArrivalUpshift;

                    if (inter_arrival->ComputeDeltas(timestamp, arrival_time_ms,
                                             payload_size, &timestamp_delta,
                                             &time_delta, &size_delta)) {

                        timestamp_delta_ms = timestamp_delta * 1000.0/static_cast<double>(1 << kInterArrivalShift);;
                        estimator.Update(time_delta, timestamp_delta_ms, size_delta, detector.State());
                        detector.Detect(estimator.offset(), timestamp_delta_ms, estimator.num_of_deltas(), arrival_time_ms);

                        num_of_deltas = estimator.num_of_deltas();
                        offset = estimator.offset();



                    }
                    ++num_datas;
            }
            if(last_t_v->data[i].row >= 0){
                        const double min_diff = 1e-6;
                        double diffT;
                        diffT = timestamp_delta_ms - rtp_time_delta_v->data[i].value;
                        if(fabs(diffT) > min_diff){
                            dbge("timestamp_delta_ms: diffT = %.9f !!!!!!", diffT);
                            ret = -1;
                            break;
                        }

                        diffT = size_delta - size_delta_v->data[i].value;
                        if(fabs(diffT) > min_diff){
                            dbge("size_delta: diffT = %f (%d, %f) !!!!!!", diffT, size_delta, size_delta_v->data[i].value);
                            ret = -1;
                            break;
                        }

                        diffT = time_delta - time_delta_v->data[i].value;
                        if(fabs(diffT) > min_diff){
                            dbge("time_delta: diffT = %.9f !!!!!!", diffT);
                            ret = -1;
                            break;
                        }

                        const double T = std::min(num_of_deltas, 60) * offset;
                        diffT = T - last_t_v->data[i].value;
                        if(fabs(diffT) > 1e-6){
                            dbge("lastT diffT = %f !!!!!!", diffT);
                            ret = -1;
                            break;
                        }

                        dumplen = sprintf(dumpbuf, "%lld, %f, %f\n", now_ms, last_t_v->data[i].value , T);
                        fwrite(dumpbuf, dumplen, 1, fp1);

            }

        }
        dbgi("process data num %d", num_datas);

    }while(0);
    if(inter_arrival){
        delete inter_arrival;
        inter_arrival = NULL;
    }

    return ret;
}

int single_verify_offset_process(FILE * fp1, xvector_array *arr){
    int ret = 0;

    const OverUseDetectorOptions& options = OverUseDetectorOptions();
    OveruseEstimator estimator(options);
    OveruseDetector detector(options);
    InterArrival * inter_arrival = new InterArrival(90 * kTimestampGroupLengthMs, kTimestampToMs, true);

    do{

        const char * dump_var_names[] = { "LastT", "LastTEst", NULL };
        xvector * timev = &arr->vectors[0];
        int i = 0;
        int nvars = 0;
        ret = dump_header(fp1, timev, dump_var_names, i, nvars);


        int vector_index = -1;
        

        // DEFINE_FIND_VECTOR(rtp_time_v, "rtp_ts", arr, vector_index);
        // DEFINE_FIND_VECTOR(rtp_arrt_v, "arrival", arr, vector_index);
        // DEFINE_FIND_VECTOR(rtp_plsize_v, "size", arr, vector_index); 

        DEFINE_FIND_VECTOR(rtp_ssrc_v, "RtpSsrc", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_time_v, "RtpTimestamp", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_arrt_v, "RtpArriTime", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_plsize_v, "RtpPlsize", arr, vector_index); 
        DEFINE_FIND_VECTOR(time_delta_v, "TimeDelta", arr, vector_index);
        DEFINE_FIND_VECTOR(rtp_time_delta_v, "RtpTimeDelta", arr, vector_index);
        DEFINE_FIND_VECTOR(size_delta_v, "SizeDelta", arr, vector_index);
        DEFINE_FIND_VECTOR(last_t_v, "LastT", arr, vector_index);
        // DEFINE_FIND_VECTOR(t_ts_delta_v, "TTSDelta", arr, vector_index);

        
        uint32_t timestamp_delta = 0;
        int64_t time_delta = 0;
        int size_delta = 0;
        double timestamp_delta_ms = 0;
        double t_ts_delta = 0;
        int num_of_deltas = 0;
        double offset = 0;

        char dumpbuf[512];
        int dumplen = 0;
        int num_datas = 0;
        for(; i < timev->num_rows; i++){
            if(timev->data[i].row < 0){
                // dbgi("skip row %d", i);
                continue;
            }

            dbgi("process row %d", i);
            int64_t now_ms = timev->data[i].value;
            
            const uint32_t main_ssrc = 1843363638;

            if(rtp_arrt_v->data[i].row >= 0){
                
                uint32_t ssrc = rtp_ssrc_v->data[i].value;
                if(ssrc == main_ssrc){
                    uint32_t timestamp = rtp_time_v->data[i].value;
                    int64_t arrival_time_ms = rtp_arrt_v->data[i].value;
                    size_t payload_size = rtp_plsize_v->data[i].value;

                    if (inter_arrival->ComputeDeltas(timestamp, arrival_time_ms,
                                             payload_size, &timestamp_delta,
                                             &time_delta, &size_delta)) {

                        timestamp_delta_ms = timestamp_delta * kTimestampToMs;

                        estimator.Update(time_delta, timestamp_delta_ms, size_delta, detector.State());
                        detector.Detect(estimator.offset(), timestamp_delta_ms, estimator.num_of_deltas(), arrival_time_ms);

                        num_of_deltas = estimator.num_of_deltas();
                        offset = estimator.offset();

                        // const double T = std::min(num_of_deltas, 60) * offset;
                        // dumplen = sprintf(dumpbuf, "%lld, %f, %f\n", now_ms, 10.0 , T);
                        // fwrite(dumpbuf, dumplen, 1, fp1);
                    }
                    ++num_datas;
                }
            }
            if(last_t_v->data[i].row >= 0){
                uint32_t ssrc = rtp_ssrc_v->data[i].value;
                if(ssrc == main_ssrc){
                        const double min_diff = 1e-6;
                        double diffT;
                        diffT = timestamp_delta_ms - rtp_time_delta_v->data[i].value;
                        if(fabs(diffT) > min_diff){
                            dbge("timestamp_delta_ms: diffT = %.9f !!!!!!", diffT);
                            ret = -1;
                            break;
                        }

                        diffT = size_delta - size_delta_v->data[i].value;
                        if(fabs(diffT) > min_diff){
                            dbge("size_delta: diffT = %f (%d, %f) !!!!!!", diffT, size_delta, size_delta_v->data[i].value);
                            ret = -1;
                            break;
                        }

                        diffT = time_delta - time_delta_v->data[i].value;
                        if(fabs(diffT) > min_diff){
                            dbge("time_delta: diffT = %.9f !!!!!!", diffT);
                            ret = -1;
                            break;
                        }

                        const double T = std::min(num_of_deltas, 60) * offset;
                        diffT = T - last_t_v->data[i].value;
                        if(fabs(diffT) > 1e-6){
                            dbge("lastT diffT = %f !!!!!!", diffT);
                            ret = -1;
                            break;
                        }

                        dumplen = sprintf(dumpbuf, "%lld, %f, %f\n", now_ms, last_t_v->data[i].value , T);
                        fwrite(dumpbuf, dumplen, 1, fp1);
                    }

            }

        }
        dbgi("process data num %d", num_datas);

    }while(0);
    if(inter_arrival){
        delete inter_arrival;
        inter_arrival = NULL;
    }

    return ret;
}

int integral_process(FILE * fp1, xvector_array *arr){
    int ret = 0;

    // now_ms, OverUseDetectorOptions(), enable_burst_grouping


    IntegralWindow * iwin = new IntegralWindow(5000);
    do{

        const char * dump_var_names[] = { "TVar", "TVarIntegral", NULL };
        xvector * timev = &arr->vectors[0];
        int i = 0;
        int nvars = 0;
        ret = dump_header(fp1, timev, dump_var_names, i, nvars);

        int vector_index = -1;
        DEFINE_FIND_VECTOR(vector, "RCVarMax", arr, vector_index);
        // DEFINE_FIND_VECTOR(vector, "LastT", arr, vector_index);

        char dumpbuf[512];
        int dumplen = 0;
        int num_datas = 0;
        for(; i < timev->num_rows; i++){
            if(timev->data[i].row < 0){
                // dbgi("skip row %d", i);
                continue;
            }

            dbgi("process row %d", i);
            int64_t now_ms = timev->data[i].value;
            if(vector->data[i].row >= 0){
                iwin->update(now_ms, vector->data[i].value);
                double integral = iwin->getSum();
                dumplen = sprintf(dumpbuf, "%lld, %f, %f\n", now_ms, vector->data[i].value , integral);
                fwrite(dumpbuf, dumplen, 1, fp1);
                num_datas++;
            }

        }
        dbgi("process data num %d", num_datas);

    }while(0);
    if(iwin){
        delete iwin;
        iwin = NULL;
    }

    return ret;
}

class RangeWindow{
protected:
    int maxRanges_;
    int64_t winSize_ ;
    int64_t winError_ = 0;
    int64_t rangeDuration_;
    std::deque<std::pair<int64_t, XRange> > history_;
    bool valid_ = false;
    XRange range_;
    
    
    void compute(int64_t now_ms){
        if(history_.size() == 0){
            range_.setValue(0);
            return;
        }
        
        range_.min_ = history_.front().second.min_;
        range_.max_ = history_.front().second.max_;
        for(auto o : history_){
            if(o.second.min_ < range_.min_){
                range_.min_ = o.second.min_;
            }
            if(o.second.max_ > range_.max_){
                range_.max_ = o.second.max_;
            }
        }
    }
    bool popExpire(int64_t now_ms){
        bool pop = false;
        while(history_.size() > 0 && (now_ms - history_.front().first) > (winSize_+winError_)) {
            history_.pop_front();
            pop = true;
        }
        if(pop){
            compute(now_ms);
        }
        return pop;
    }
public:
    RangeWindow(int maxRanges, int64_t winSize=5000, int64_t winError = 500)
    :maxRanges_(maxRanges), winSize_(winSize), winError_(winError), rangeDuration_(winSize_/maxRanges), range_(0){
    }
    bool update(int64_t now_ms, double value){
        if(history_.size() == 0 || (now_ms - history_.back().first) >= rangeDuration_ ){
            history_.push_back(std::make_pair(now_ms, XRange(value)  ) );
        }else{
            if(value > history_.back().second.max_){
                history_.back().second.max_ = value;
            }
            if(value < history_.back().second.min_){
                history_.back().second.min_ = value;
            }
        }
        if(!popExpire(now_ms)){
            if(value > range_.max_){
                range_.max_ = value;
            }
            if(value < range_.min_){
                range_.min_ = value;
            }
        }

        if((now_ms - history_.front().first) >= (winSize_-winError_)) {
            valid_ = true;
        }
        return valid_;
    }
    bool valid(){
        return valid_;
    }
    double max(){
        return range_.max_;
    }
    double min(){
        return range_.min_;
    }
    double range(){
        return range_.max_ - range_.min_;
    }
};

int range_process(FILE * fp1, xvector_array *arr){
    int ret = 0;
    RangeWindow * iwin = new RangeWindow(5000);
    do{
        const char * dump_var_names[] = { "LastT", "RangeT", NULL };
        xvector * timev = &arr->vectors[0];
        int i = 0;
        int nvars = 0;
        ret = dump_header(fp1, timev, dump_var_names, i, nvars);

        int vector_index = -1;
        DEFINE_FIND_VECTOR(vector, "LastT", arr, vector_index);

        char dumpbuf[512];
        int dumplen = 0;
        int num_datas = 0;
        for(; i < timev->num_rows; i++){
            if(timev->data[i].row < 0){
                // dbgi("skip row %d", i);
                continue;
            }

            dbgi("process row %d", i);
            int64_t now_ms = timev->data[i].value;
            if(vector->data[i].row >= 0){
                if(iwin->update(now_ms, vector->data[i].value)){
                    double diff = iwin->range();
                    dumplen = sprintf(dumpbuf, "%lld, %f, %f\n", now_ms, vector->data[i].value , diff);
                    fwrite(dumpbuf, dumplen, 1, fp1);
                    num_datas++;
                }

            }

        }
        dbgi("process data num %d", num_datas);

    }while(0);
    if(iwin){
        delete iwin;
        iwin = NULL;
    }

    return ret;
}

class HistoryWin{
protected:
    int64_t winSize_ ;
    int64_t winError_ = 0;
    bool valid_ = false;
    double sum_ = 0;
    std::deque<std::pair<int64_t, double> > history_;

    void popExpire(int64_t now_ms){
        while(history_.size() > 0 && (now_ms - history_.front().first) > (winSize_+winError_)) {
            sum_ -= history_.front().second;
            history_.pop_front();
        }
    }
public:
    HistoryWin(int64_t winSize=5000, int64_t winError = 500):winSize_(winSize), winError_(winError){
    }
    bool update(int64_t now_ms, double value){
        sum_ += value;
        history_.push_back(std::make_pair(now_ms, value ) );
        popExpire(now_ms);
        if((now_ms - history_.front().first) >= (winSize_-winError_)) {
            valid_ = true;
        }
        return true;
    }
    double getAverage(int64_t now_ms, bool autoPop=true){
        if(!valid_) return 0;
        if(now_ms > 0 && autoPop){
            popExpire(now_ms);
        }
        if(history_.size() > 0){
            return sum_/history_.size();
        }else{
            return 0;
        }
    }
    double getTailDiffHead(int64_t now_ms, bool autoPop=true){
        if(history_.size() > 0){
            return history_.back().second - history_.front().second;
        }else{
            return 0;
        }
    }
};




int history_process(FILE * fp1, xvector_array *arr){
    int ret = 0;

    // now_ms, OverUseDetectorOptions(), enable_burst_grouping


    HistoryWin * iwin = new HistoryWin(5000);
    do{

        const char * dump_var_names[] = { "TVar", "TVarIntegral", NULL };
        xvector * timev = &arr->vectors[0];
        int i = 0;
        int nvars = 0;
        ret = dump_header(fp1, timev, dump_var_names, i, nvars);

        int vector_index = -1;
        DEFINE_FIND_VECTOR(vector, "NormIntegral", arr, vector_index);

        char dumpbuf[512];
        int dumplen = 0;
        int num_datas = 0;
        for(; i < timev->num_rows; i++){
            if(timev->data[i].row < 0){
                // dbgi("skip row %d", i);
                continue;
            }

            dbgi("process row %d", i);
            int64_t now_ms = timev->data[i].value;
            if(vector->data[i].row >= 0){
                if(iwin->update(now_ms, vector->data[i].value)){
                    double diff = iwin->getTailDiffHead(now_ms);
                    dumplen = sprintf(dumpbuf, "%lld, %f, %f\n", now_ms, vector->data[i].value , diff);
                    fwrite(dumpbuf, dumplen, 1, fp1);
                    num_datas++;
                }

            }

        }
        dbgi("process data num %d", num_datas);

    }while(0);
    if(iwin){
        delete iwin;
        iwin = NULL;
    }

    return ret;
}


int main_abs(int argc, char ** argv){
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

    xvector_array *arr = load_txt(input_filename); 
    if(!arr){
        dbge("fail to load [%s]", input_filename);
        return -1;    
    }
    dbgi("loaded input: [%s]", input_filename); 
    dbgi("vectors=%d, rows=%d",  arr->num_vectors, arr->vectors[0].num_rows); 

    
    FILE * fp1 = NULL;
    ret = -1;
    do{
        if(arr->num_vectors <= 1){
            dbge("vectors <= 1");
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

        // ret = abs_integral_process(fp1, arr);
        // ret = abs_verify_recvrate_process(fp1, arr);
        // ret = abs_verify_offset_process(fp1, arr);
        // ret = single_verify_offset_process(fp1, arr);
        // ret = integral_process(fp1, arr);
        ret = range_process(fp1, arr);
        // ret = history_process(fp1, arr);
        if(ret) break;

        ret = 0;
        dbgi("done");
    }while(0);

    if(fp1){
        fclose(fp1);
        fp1 = NULL;
    }

    if(arr){
        xvector_array_free(arr);
        arr = NULL;
    }

    return 0;
}


// double average_packet_size(int current_bitrate_bps_){
//     // int current_bitrate_bps_ = 300*1000;
//     double bits_per_frame = static_cast<double>(current_bitrate_bps_) / 30.0;
//     double packets_per_frame = std::ceil(bits_per_frame / (8.0 * 1200.0));
//     double avg_packet_size_bits = bits_per_frame / packets_per_frame;
//     dbgi("bitrate %d, bits_per_frame=%f, packets_per_frame=%f, avg_packet_size_bits=%f", current_bitrate_bps_, bits_per_frame, packets_per_frame, avg_packet_size_bits);
//     return avg_packet_size_bits;
// }
int main(int argc, char ** argv){
    // int bitrates[] = {300*1000, 350*1000, 400*1000, 445211, 749380, 752946, 10*1000*1000, 100*1000*1000};
    // for(int i = 0; i < sizeof(bitrates)/sizeof(bitrates[0]); i++){
    //     average_packet_size(bitrates[i]);
    // }
    

    // return main_single(argc, argv);
    return main_abs(argc, argv);
}




