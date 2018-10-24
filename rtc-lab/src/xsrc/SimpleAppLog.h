#ifndef __SimpleAppLog_h__
#define __SimpleAppLog_h__

#include <stdio.h>
#include "plw_defs.h"

#ifdef  __cplusplus
extern "C" {
#endif

void SimplePrintTime();
void SimpleLogPrintBuf(const char * prefix, const void * buf, plw_u32 buflen);
void SimpleLogStartFlush(plw_u32 inerval_ms);
void SimpleLogStopFlush();

#define DEBUG_APP

#ifdef DEBUG_APP

#define sdbgd(...) do{ SimplePrintTime(); printf("[DEBUG] " __VA_ARGS__); printf("\n");}while(0)
#define sdbgi(...) do{ SimplePrintTime(); printf("[INFO ] " __VA_ARGS__); printf("\n");}while(0)
#define sdbgw(...) do{ SimplePrintTime(); printf("[WARN ] " __VA_ARGS__); printf("\n");}while(0)
#define sdbge(...) do{ SimplePrintTime(); printf("[ERROR] " __VA_ARGS__); printf("\n");}while(0)
#define sdbgbuf(xpre, xbuf, xbuflen)  SimpleLogPrintBuf((xpre), (xbuf), (xbuflen))
#else
#define sdbgi(...) 
#define sdbgw(...) 
#define sdbge(...) 
#define sdbgbuf(...)
#endif

#ifdef  __cplusplus
}
#endif


#endif
