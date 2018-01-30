
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xcmdline.h"


// xcmdline implementation
// typedef struct xcmdline{
//     xcmd_option * options;
//     int size;
// }xcmdline;

static 
const char * short_options_from_long(const struct option *long_options, int size, char *short_options){
        int len = 0;
        for(int i = 0; i < size; i++){
            const struct option * opt = &long_options[i];
            if(opt->val < XOPT_EXT_BASE){
                short_options[len] = opt->val; ++ len;
                if(opt->has_arg == required_argument){
                    short_options[len] = ':'; ++len;
                }else if(opt->has_arg == optional_argument){
                    short_options[len] = ':'; ++len;
                    short_options[len] = ':'; ++len;
                }
            }
        }
        short_options[len] = '\0';
        return short_options;
}

static
int find_option(const struct xcmd_option * options, int size, int c){
    for(int i = 0; i < size; i++){
        if(c == options[i].opt.val){
            return i;
        }
    }
    return -1;
}

static 
int assign_xopt_from_raw(const struct xcmd_option * xopt, xoptval * xval, const char * str){
    xval->raw = str;
    if(str){
        if(xopt->typ == XOPTTYPE_STR){
            xval->strval = str;
        }else if(xopt->typ == XOPTTYPE_INT){
            xval->intval = atoi(str);
        }else if(xopt->typ == XOPTTYPE_DOUBLE){
            xval->doubleval = atof(str);
        }
    }

    return 0;
}

void xcmdline_init_options(struct xcmd_option * options, int size){
    memset(options, 0, size*sizeof(xcmd_option));
    for(int i = 0; i < size; i ++){
        options[i].mandatory = 0;
    }
}

int xcmdline_parse(int argc, char ** argv, const struct xcmd_option * options, int size, xoptval * out, char errmsg[]){
    struct option long_options[size+1];
    char short_options[size * 3+1]; 
    int short_len = 0;

    memset(long_options, 0, sizeof(long_options));
    for(int i = 0; i < size; i++){
        long_options[i] = options[i].opt; // build long options
        out[i] = options[i].def_val;  // init out to default
        // out[i].raw = NULL;
    }
    short_options_from_long(long_options, size, short_options);

    // dump long and short 
    // for(int i = 0; i < size+1; i++){
    //     const struct option * lopt = &long_options[i];
    //     dbgi("long_options[%d]:  name=[%s], has_arg=[%d], flag=[%p], val=[%d] ", i, lopt->name, lopt->has_arg, lopt->flag, lopt->val);
    // }
    // dbgi("short_options = [%s] ", short_options);

    // parse command line
    // see https://www.gnu.org/software/libc/manual/html_node/Getopt.html#Getopt
    // see https://www.gnu.org/software/libc/manual/html_node/Getopt-Long-Option-Example.html
    int c;
    opterr = 0;
    int option_index = 0;
    while ((c = getopt_long (argc, argv, short_options, long_options, &option_index)) != -1){
        if(c == '?'){
            return -1;
        }

        int index = find_option(options, size, c);
        if(index < 0){
            return -1;
        }

        const struct xcmd_option * xopt = &options[index];
        xoptval * xval = &out[index];

        if(xopt->opt.has_arg == no_argument){
            xval->raw = "1";
            xval->intval = 1;
        }else if(xopt->opt.has_arg == required_argument){
            assign_xopt_from_raw(xopt, xval, optarg);
        }else if(xopt->opt.has_arg == optional_argument){
            assign_xopt_from_raw(xopt, xval, optarg);
        }else{
            // fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            return -1;
        }
    }

    // TODO: check mandatory option
    
    return 0;
}

const char * xcmdline_get_usage(int argc, char **argv, const struct xcmd_option * options, int size){
    static char usage[16*1024];
    int len = 0;
    len += sprintf(usage + len, "Usage: %s [options]\n", argv[0]);
    for(int i = 0; i < size; i++){
        const struct xcmd_option * xopt = &options[i]; 
        if(xopt->opt.val < XOPT_EXT_BASE){
            len += sprintf(usage + len, "  -%c, --%s  %s\n", xopt->opt.val, xopt->opt.name, xopt->short_desc);   
        }else{
            len += sprintf(usage + len, "  --%s  %s\n", xopt->opt.name, xopt->short_desc); 
        }
        if(xopt->def_val.raw){
            len += sprintf(usage + len, "      default %s\n", xopt->def_val.raw);  
        }
        if(xopt->long_desc){
            len += sprintf(usage + len, "      %s\n", xopt->long_desc);     
        }
        len += sprintf(usage + len, "\n");
    }
    return usage;
}


const char *  xcmdline_get_config_string(const struct xcmd_option * options, int size, const xoptval * config){
    static char buf[16*1024];
    int len = 0;
    for(int i = 0; i < size; i++){
        
        const struct xcmd_option * xopt = &options[i]; 
        const xoptval * xval = &config[i];
        // dbgi("===[%s]", xval->raw);
        if(xval->raw){
            len += sprintf(buf + len, "  opt: %s=[%s]\n", xopt->opt.name, xval->raw);    
        }
    }
    // dbgi("===%s", buf);
    return buf;
}


// xcmdline_t xcmdline_create(int size){
//     xcmdline_t obj = (xcmdline_t)malloc(sizeof(xcmdline));
//     obj->size = size;
//     obj->options = (xcmd_option *)malloc(sizeof(xcmd_option) * obj->size);
//     return obj;
// }

// void xcmdline_delete(xcmdline_t obj){
//     if(!obj) return;
//     if(obj->options){
//         free(obj->options);
//         obj->options = NULL;
//     }
//     free(obj);
// }

