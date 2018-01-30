#include "plw.h"


plw_u32         plw_tcp_send_sync
                (
                plw_tcp_t           tcp,
                const void *        data_ptr,
                const plw_u32       len
                )
{
    plw_u32             ret = 0;
    plw_u32             xfer = 0;
    plw_u32             bytes = 0;
    plw_u8 *            ptr = (plw_u8 *) data_ptr;
    plw_u32             w = 0;

    while(xfer < len)
    {
        ret = plw_tcp_send(tcp, ptr+xfer, len-xfer, &bytes);
        if(ret)
        {
            ret = PLWS_ERROR;
            break;
        }

        if(bytes == 0)
        {
            w++;
            if(w > 3)
            {
                ret = PLWS_ERROR;
                break;
            }
            plw_delay(10);
        }

        xfer += bytes;
    }

    return ret;
}


plw_u32         plw_tcp_recv_sync
                (
                plw_tcp_t           tcp,
                const void *        data_ptr,
                const plw_u32       len
                )
{
    plw_u32             ret = 0;
    plw_u32             xfer = 0;
    plw_u32             bytes = 0;
    plw_u8 *            ptr = (plw_u8 *) data_ptr;
    plw_u32             w = 0;

    while(xfer < len)
    {
        ret = plw_tcp_recv(tcp, ptr+xfer, len-xfer, &bytes, 0, 0);
        if(ret )
        {
            ret = PLWS_ERROR;
            break;
        }

        if(bytes == 0)
        {
            w++;
            if(w > 3)
            {
                ret = PLWS_ERROR;
                break;
            }
            plw_delay(10);
        }

        xfer += bytes;
    }

    return ret;
}

