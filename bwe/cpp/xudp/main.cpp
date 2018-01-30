
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
#include <chrono>

#include "xcmdline.h"
#include "plw.h"


#define dbgv(...) do{  printf("<main>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{  printf("<main>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{  printf("<main>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)


static
int64_t get_timestamp_ms(){ 
    unsigned long milliseconds_since_epoch = 
    std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();
    return milliseconds_since_epoch;
}


// app implementation

enum XOPTS {
    XOPT_HELP = 0,
    XOPT_LOCAL_PORT ,
    XOPT_REMOTE_IP ,
    XOPT_REMOTE_PORT,
    XOPT_DURATION,
    XOPT_BITRATE,
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

        app_options[XOPT_LOCAL_PORT] = (struct xcmd_option){
            .typ = XOPTTYPE_INT,
            .mandatory = 0,
            .opt = { "local-port",  required_argument,  NULL, 'l' },
            .short_desc = "<local-port>", 
            .long_desc = "local socket port;",
            .def_val = {.intval = 0, .raw = "0"}
            };

        app_options[XOPT_REMOTE_IP] = (struct xcmd_option){
            .typ = XOPTTYPE_STR,
            .mandatory = 0,
            .opt = { "remote-ip", required_argument,  NULL, 'r' },
            .short_desc = "<remote-ip>", 
            .long_desc = "remote server ip;",
            .def_val = {.strval = "", .raw = ""}
            };

        app_options[XOPT_REMOTE_PORT] = (struct xcmd_option){
            .typ = XOPTTYPE_INT,
            .mandatory = 0,
            .opt = { "remote-port",  required_argument,  NULL, 'p' },
            .short_desc = "<remote-port>", 
            .long_desc = "remote server port;",
            .def_val = {.intval = 0, .raw = "0"}
            };

        app_options[XOPT_DURATION] = (struct xcmd_option){
            .typ = XOPTTYPE_INT,
            .mandatory = 0,
            .opt = { "duration",  required_argument,  NULL, 'd' },
            .short_desc = "<duration>", 
            .long_desc = "duration in seconds;",
            .def_val = {.intval = 10, .raw = "10"}
            };

        app_options[XOPT_BITRATE] = (struct xcmd_option){
            .typ = XOPTTYPE_INT,
            .mandatory = 0,
            .opt = { "bitrate",  required_argument,  NULL, 'b' },
            .short_desc = "<bitrate>", 
            .long_desc = "bitrate in kbps;",
            .def_val = {.intval = 200, .raw = "200"}
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

    plw_port_t local_port = configs[XOPT_LOCAL_PORT].intval;
    const char * remote_host = configs[XOPT_REMOTE_IP].strval;;
    plw_port_t remote_port = configs[XOPT_REMOTE_PORT].intval;
    int64_t duration_ms = configs[XOPT_DURATION].intval*1000;
    int bitrate = configs[XOPT_BITRATE].intval*1000;
    int pkt_size = 512;
    plw_udp_t udp = 0;
    
    do{
        ret = plw_udp_create(0, &local_port, 0, 0, &udp);
        if(ret){
            dbge("fail to open udp at port %d", local_port);
            break;
        }
        dbgi("open udp at port %d", local_port);

        // dbgi("bitrate=%d, duration=%lldms remote=%s:%d", bitrate, duration_ms, remote_host, remote_port);
        int64_t start_ms = get_timestamp_ms(); 
        int64_t last_ms = 0; 
        int64_t sent_bytes = 0;
        uint8_t buf[pkt_size];
        while((get_timestamp_ms() - start_ms) < duration_ms){
            int64_t next_ms = 1000*(sent_bytes*8)/bitrate;
            int64_t elapse = (get_timestamp_ms() - start_ms);
            // dbgi("next_ms=%lld, elapse=%lld", next_ms, elapse);
            if(elapse >= next_ms){
                plw_udp_send(udp, remote_host, remote_port, buf, pkt_size, NULL);
                sent_bytes += pkt_size;

            }else{
                int d = next_ms - elapse;
                // dbgi("sleep %d ms", d);
                plw_delay(d);
            }
            if((elapse - last_ms) >= 1000){
                int64_t br = 1000*(sent_bytes*8)/elapse;
                dbgi("remote=%s:%d, time=%lld/%lld, bitrate=%lld", remote_host, remote_port, elapse, duration_ms, br);
                last_ms = elapse;
            }
        }

        dbgi("done");
    }while(0);

    if(udp){
        plw_udp_delete(udp);
        udp = 0;
    }

    return 0;
}




