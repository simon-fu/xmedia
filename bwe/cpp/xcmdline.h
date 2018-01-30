
#ifndef  _XCMDLINE_H
#define  _XCMDLINE_H

#include <getopt.h>

#ifdef __cplusplus
extern "C" {
#endif

// xcmdline interface 
enum XOPTTYPE {
    XOPTTYPE_NONE = 0,
    XOPTTYPE_STR ,
    XOPTTYPE_INT ,
    // XOPTTYPE_FLOAT ,
    XOPTTYPE_DOUBLE ,
};

#define XOPT_EXT_BASE 256

typedef struct {
    const char * raw; 
    union{
        const char * strval;
        int intval;
        // float floatval;
        double doubleval;
    };
}xoptval;

typedef struct xcmd_option{
    int typ; // XOPTTYPE
    int mandatory;
    struct option opt;
    const char * short_desc;
    const char * long_desc;
    xoptval def_val;
}xcmd_option;

// typedef struct xcmdline * xcmdline_t;

void xcmdline_init_options(struct xcmd_option * options, int size);

int xcmdline_parse(int argc, char ** argv, const struct xcmd_option * options, int size, xoptval * out, char errmsg[]);

const char * xcmdline_get_usage(int argc, char **argv, const struct xcmd_option * options, int size);

const char *  xcmdline_get_config_string(const struct xcmd_option * options, int size, const xoptval * config);



#ifdef __cplusplus
}
#endif

#endif