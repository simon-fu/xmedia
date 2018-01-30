

#ifndef __PLW_DEFS_H__
#define __PLW_DEFS_H__

#ifndef __STDC_LIMIT_MACROS 
#define __STDC_LIMIT_MACROS 
#endif

#include<stddef.h>



//#include <stdint.h>
//#include <limits.h>

#ifdef __cplusplus  
extern "C" {
#endif  

typedef unsigned char       plw_u8;

typedef unsigned short      plw_u16;

typedef unsigned int        plw_u32;

typedef unsigned long long  plw_u64;



typedef signed char         plw_s8;

typedef signed short int    plw_s16;

typedef signed long int     plw_s32;

typedef long long  plw_s64;
//typedef int64_t   plw_s64;	



typedef unsigned char       plw_bool;

#define PLW_TRUE               1

#define PLW_FALSE              0




typedef plw_u32              plw_size;

typedef char                plw_char;



#ifndef PLW_NULL
#define PLW_NULL    ((void *)0)
#endif


#define PLW_ENDIAN_LITTLE



#define PLW_LLONG_MAX   0x7fffffffffffffff // INT64_MAX //  9223372036854775807LL   /* maximum signed long long int value */
#define PLW_LLONG_MIN   (-0x7fffffffffffffff - 1) //INT64_MIN // -9223372036854775808LL    /* minimum signed long long int value */
#define PLW_ULLONG_MAX  0xffffffffffffffffU //UINT64_MIN   //0xffffffffffffffffULL   /* maximum unsigned long long int value */

//#define PLW_LLONG_MAX     9223372036854775807i64       /* maximum signed long long int value */
//#define PLW_LLONG_MIN   (-9223372036854775807i64 - 1)  /* minimum signed long long int value */
//#define PLW_ULLONG_MAX    0xffffffffffffffffui64       /* maximum unsigned long long int value */

#define PLW_ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#ifdef __cplusplus  
}
#endif  

#endif




