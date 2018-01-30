


#ifndef __VENDIAN_H__
#define __VENDIAN_H__

#include "plw_defs.h"



/*
* big-endian read/write
*/


#define     be_get_u16(xptr)   ((plw_u16) ( ((xptr)[0] << 8) | ((xptr)[1] << 0) ))     /* by simon */
#define     be_set_u16(xval, xptr)  \
do{\
    (xptr)[0] = (plw_u8) (((xval) >> 8) & 0x000000FF);\
    (xptr)[1] = (plw_u8) (((xval) >> 0) & 0x000000FF);\
}while(0)



#define     be_get_u24(xptr)   ((plw_u32) ( ((xptr)[0] << 16) | ((xptr)[1] << 8) | ((xptr)[2] << 0)))      /* by simon */
#define     be_set_u24(xval, xptr)  \
do{\
    (xptr)[0] = (plw_u8) (((xval) >> 16) & 0x000000FF);\
    (xptr)[1] = (plw_u8) (((xval) >> 8) & 0x000000FF);\
    (xptr)[2] = (plw_u8) (((xval) >> 0) & 0x000000FF);\
}while(0)




#define     be_get_u32(xptr)   ((plw_u32) ((((plw_u32)(xptr)[0]) << 24) | ((xptr)[1] << 16) | ((xptr)[2] << 8) | ((xptr)[3] << 0)))        /* by simon */
#define     be_set_u32(xval, xptr)  \
do{\
    (xptr)[0] = (plw_u8) (((xval) >> 24) & 0x000000FF);\
    (xptr)[1] = (plw_u8) (((xval) >> 16) & 0x000000FF);\
    (xptr)[2] = (plw_u8) (((xval) >> 8) & 0x000000FF);\
    (xptr)[3] = (plw_u8) (((xval) >> 0) & 0x000000FF);\
}while(0)




/*
* little-endian read/write
*/

#define     le_get_u16(xptr)   (plw_u16) ( ((xptr)[1] << 8) | ((xptr)[0] << 0) )
#define     le_set_u16(xval, xptr)  \
do{\
    (xptr)[1] = (plw_u8) (((xval) >> 8) & 0x000000FF);\
    (xptr)[0] = (plw_u8) (((xval) >> 0) & 0x000000FF);\
}while(0)


#define     le_get_u24(xptr)   (plw_u16) ( ((xptr)[2] << 16) | ((xptr)[1] << 8) | ((xptr)[0] << 0) )
#define     le_set_u24(xval, xptr)  \
do{\
    (xptr)[2] = (plw_u8) (((xval) >> 16) & 0x000000FF);\
    (xptr)[1] = (plw_u8) (((xval) >> 8) & 0x000000FF);\
    (xptr)[0] = (plw_u8) (((xval) >> 0) & 0x000000FF);\
}while(0)



#define     le_get_u32(xptr)   (plw_u32) ((xptr)[3] << 24) | ((xptr)[2] << 16) | ((xptr)[1] << 8) | ((xptr)[0] << 0)
#define     le_set_u32(xval, xptr)  \
do{\
    (xptr)[3] = (plw_u8) (((xval) >> 24) & 0x000000FF);\
    (xptr)[2] = (plw_u8) (((xval) >> 16) & 0x000000FF);\
    (xptr)[1] = (plw_u8) (((xval) >> 8) & 0x000000FF);\
    (xptr)[0] = (plw_u8) (((xval) >> 0) & 0x000000FF);\
}while(0)






#define endian_u16_swap(x)     ( ((x) << 8) & 0xff00) | ( ((x) >> 8) & 0x00ff) )
#define endian_u32_swap(x)     ( ( ((x) << 24) & 0xff000000) | (((x) << 16) & 0x00ff0000) |  (((x) >> 16) & 0x0000ff00) | (((x) >> 24) & 0x000000ff))







#ifdef PLW_ENDIAN_LITTLE

#define endian_u16_big_to_host(x)       endian_u16_swap(x)
#define endian_u32_big_to_host(x)       endian_u32_swap(x)

#define endian_u16_little_to_host(x)    (x)
#define endian_u32_little_to_host(x)    (x)

#else

#define endian_u16_big_to_host(x)       (x)
#define endian_u32_big_to_host(x)       (x)

#define endian_u16_little_to_host(x)    endian_u16_swap(x)
#define endian_u32_little_to_host(x)    endian_u32_swap(x)

#endif




#endif


