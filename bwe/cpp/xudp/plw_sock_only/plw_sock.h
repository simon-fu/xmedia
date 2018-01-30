

/* plw_sock.h */

#ifndef __PLW_SOCK_H__
#define __PLW_SOCK_H__


#include "plw_defs.h"
#include "plw_drv_sock.h"


#ifdef __cplusplus  
extern "C" {
#endif  


typedef plw_drv_udp_t   plw_udp_t;


/* plw_u32 init_sock(void); */

/* plw_u32 terminate_sock(void); */







#define plw_udp_create(xlocal_ip, xpport, xsend_buf_size, xrecv_buf_size, xpudp) \
            plw_drv_udp_create(xlocal_ip, xpport, xsend_buf_size, xrecv_buf_size, xpudp)


#define plw_udp_delete(xudp) plw_drv_udp_delete(xudp)


#define plw_udp_close(xudp) plw_drv_udp_close(xudp)

#define plw_udp_recv(xudp, xdata, xbytes_to_recv, xbytes_received, xremote_ip, xremote_port) \
            plw_drv_udp_recv(xudp, xdata, xbytes_to_recv, xbytes_received, xremote_ip, xremote_port)


#define plw_udp_send(xudp, xremote_ip, xremote_port, xdata, xbytes_to_send, xbytes_sent) \
            plw_drv_udp_send(xudp, xremote_ip, xremote_port, xdata, xbytes_to_send, xbytes_sent)


#define plw_udp_recv_timeout(xudp, xtimeout_ms, xdata, xbytes_to_recv, xbytes_received, xremote_ip, xremote_port)             plw_drv_udp_recv_timeout(xudp, xtimeout_ms, xdata, xbytes_to_recv, xbytes_received, xremote_ip, xremote_port)

#define plw_udp_broadcast(xudp, xremote_ip, xremote_port, xdata, xbytes_to_send, xbytes_sent)             plw_drv_udp_broadcast(xudp, xremote_ip, xremote_port, xdata, xbytes_to_send, xbytes_sent)




typedef plw_drv_tcp_t   plw_tcp_t;


#define plw_tcp_create(xlocal_ip, xpport, xsend_buf_size, xrecv_buf_size, xlisten_conn_count, xtcp) \
            plw_drv_tcp_create(xlocal_ip, xpport, xsend_buf_size, xrecv_buf_size, xlisten_conn_count, xtcp)

#define plw_tcp_delete(xtcp)    plw_drv_tcp_delete(xtcp)

#define plw_tcp_close(xtcp)     plw_drv_tcp_close(xtcp)

#define plw_tcp_recv(xtcp, xdata, xbytes_to_recv, xbytes_received, xremote_ip, xremote_port) \
            plw_drv_tcp_recv(xtcp, xdata, xbytes_to_recv, xbytes_received, xremote_ip, xremote_port)

#define plw_tcp_recv_timeout(xtcp, xtimeout_ms, xdata, xbytes_to_recv, xbytes_received, xremote_ip, xremote_port) \
            plw_drv_tcp_recv_timeout(xtcp, xtimeout_ms, xdata, xbytes_to_recv, xbytes_received, xremote_ip, xremote_port)
            
#define plw_tcp_send(xtcp, xdata, xbytes_to_send, xbytes_sent) \
            plw_drv_tcp_send(xtcp, xdata, xbytes_to_send, xbytes_sent)

#define plw_tcp_wait_conn(xtcp_serv, xptcp_conn, xremote_ip, xremote_port) \
            plw_drv_tcp_wait_conn(xtcp_serv, xptcp_conn, xremote_ip, xremote_port)

#define plw_tcp_conn(xtcp, xremote_ip, xremote_port, xtimeout_ms) \
            plw_drv_tcp_conn(xtcp, xremote_ip, xremote_port, xtimeout_ms)
                    

plw_u32         plw_tcp_send_sync
                (
                plw_tcp_t           tcp,
                const void *        data_ptr,
                const plw_u32       len
                );

plw_u32         plw_tcp_recv_sync
                (
                plw_tcp_t           tcp,
                const void *        data_ptr,
                const plw_u32       len
                );

#ifdef __cplusplus  
}
#endif  

#endif


/* end of file */



