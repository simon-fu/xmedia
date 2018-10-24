#include "SimpleAppLog.h"

#undef dbgd
#undef dbgi
#undef dbgw
#undef dbge
#undef dbgbuf

#ifdef MNAME
#define dbgd(...) sdbgd(MNAME __VA_ARGS__)
#define dbgi(...) sdbgi(MNAME __VA_ARGS__)
#define dbgw(...) sdbgw(MNAME __VA_ARGS__)
#define dbge(...) sdbge(MNAME __VA_ARGS__)
#define dbgbuf(buf, len) sdbgbuf(MNAME, buf, len)

#else
#define dbgd(...)
#define dbgi(...) 
#define dbgw(...) 
#define dbge(...) 
#define dbgbuf(...) 
#endif

