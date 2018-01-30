

#ifndef __PLW_H__
#define __PLW_H__



#ifdef __cplusplus  
extern "C" {
#endif  



/*************************************/
/*       other h file in vpl         */
/*************************************/

#include "plw_endian.h"
#include "plw_base.h"
#include "plw_sock.h"




typedef struct plw_buffer_tag
{
    plw_u8 *    prim_ptr;     /* only visible to owner */
	plw_u8 *    ptr;
	plw_u32     len;
}plw_buffer;


#define  _PLW_MK_STR(s)     #s
#define  PLW_MK_STR(s)      _PLW_MK_STR(s)


/*
#ifndef CONFIG_SVNVER
#define CONFIG_SVNVER 0
#endif

#define PLW_SVN_VER_STR PLW_MK_STR(CONFIG_SVNVER)
*/


typedef const char *    xret_t;


#ifdef __cplusplus  
}
#endif



#endif

/* end of file */


