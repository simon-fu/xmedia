

#include "plw_drv.h"

#include "rpl_drv_sock.h"

plw_u32 init_sock(void)
{
    plw_u32 vret = 0;
    

    return vret;
}

plw_u32 terminate_sock(void)
{
    return 0;
}

int vsock_errno(void)
{
    // sock_dbg_info("##### vsock_errno(): %s\r\n", strerror(errno));
    // sock_dbg_info("@@@@ wrn on %s, %u, err str %s\r\n",  __FILE__, __LINE__, strerror(errno));
    return errno;
}



/* end of file */

