
#ifndef __PLW_DRV_SOCK_H__
#define __PLW_DRV_SOCK_H__

#include "plw_defs.h"

#ifdef __cplusplus  
extern "C" {
#endif  

/*
 * port type 
 */

typedef plw_u16     plw_port_t ;  






/*
 * udp operations
 */

typedef struct plw_drv_udp_tag * plw_drv_udp_t;


/*
 * plw_drv_udp_create()
 * @local_ip: IN, the local ip string, PLW_NULL means any ip.
 *            OUT, get actual local ip.
 * @pport: IN, the local port to create on, PLW_NULL or 0 means any port.
 *         OUT, get actual local port.
 * @send_buff_size: IN, send buffer size, 0 means using OS default value.
 * @recv_buf_size: IN, receive buffer size, 0 means using OS default value. 
 * @pudp: OUT, the udp obj created.
 */
plw_u32 plw_drv_udp_create
                        (
                        plw_char *      local_ip, 
                        plw_port_t *    pport, 
                        plw_u32         send_buf_size,
                        plw_u32         recv_buf_size,
                        plw_drv_udp_t * pudp
                        );

/*
 * plw_drv_udp_delete()
 * @udp: IN, udp obj to be deleted.
 */
plw_u32 plw_drv_udp_delete(plw_drv_udp_t udp);


/*
 * plw_drv_udp_close()
 * @udp: IN, udp obj
 */
plw_u32 plw_drv_udp_close(plw_drv_udp_t udp);


/*
 * plw_drv_udp_recv()
 * @udp: IN, udp obj
 * @data_ptr: OUT, buffer to store the data 
 * @bytes_to_recv: IN, bytes to recieve
 * @bytes_received: OUT, the actual bytes received, it can be PLW_NULL
 * @remote_ip: OUT, the ip string of remote host which send the data, it can be PLW_NULL
 * @remote_port: OUT, the port of remoet host which send the data, it can be PLW_NULL
 *
 * this function receives udp data. If no data available, it would be blocked. If the
 * udp obj has been closed by plw_drv_udp_close(), this function return with zero 
 * bytes received (if @bytes_received valid).
 */
plw_u32 plw_drv_udp_recv
                    (
                    plw_drv_udp_t   udp, 
                    void *          data_ptr, 
                    plw_u32         bytes_to_recv, 
                    plw_u32 *       bytes_received,
                    plw_char *      remote_ip, 
                    plw_port_t *    remote_port
                    );


/* plw_drv_udp_send()
 * @udp: IN, udp obj
 * @remote_ip: IN, remote ip string, "255.255.255.255" means broadcast transfer
 * @remote_port: IN, remote port
 * @data_ptr: IN, data to be sent
 * @bytes_to_send: IN, data length
 * @bytes_sent: OUT, data in bytes had been sent, it can be PLW_NULL
 */
plw_u32 plw_drv_udp_send
                    (
                    plw_drv_udp_t   udp, 
                    const plw_char *remote_ip, 
                    plw_port_t      remote_port,
                    void *          data_ptr, 
                    plw_u32         bytes_to_send, 
                    plw_u32 *       bytes_sent
                    );


plw_u32 plw_drv_udp_recv_timeout
                    (
                    plw_drv_udp_t   udp, 
                    plw_u32         timeout_ms,
                    void *          data_ptr, 
                    plw_u32         bytes_to_recv, 
                    plw_u32 *       bytes_received,
                    plw_char *      remote_ip, 
                    plw_port_t *    remote_port
                    );

plw_u32 plw_drv_udp_broadcast
                    (
                    plw_drv_udp_t       udp, 
                    plw_char *    remote_ip, 
                    plw_port_t          remote_port,
                    void *              data_ptr, 
                    plw_u32             bytes_to_send, 
                    plw_u32 *           bytes_sent
                    );




/*
 * tcp operations
 */

 
typedef struct plw_drv_tcp_tag * plw_drv_tcp_t;

/*
 * plw_drv_tcp_create()
 * @local_ip: IN, the local ip string, PLW_NULL means any ip.
 *            OUT, get actual local ip.
 * @pport: IN, the local port to create on, PLW_NULL or 0 means any port.
 *         OUT, get actual local port.
 * @send_buff_size: IN, send buffer size, 0 means using OS default value.
 * @recv_buf_size: IN, receive buffer size, 0 means using OS default value. 
 * @listen_conn_count: IN, for server, it is non-zero for listen connection count, 
 *                     for client, it must be set to 0.
 * @ptcp: OUT, the tcp obj created.
 */
plw_u32 plw_drv_tcp_create
                        (
                        plw_char *      local_ip, 
                        plw_port_t *    pport, 
                        plw_u32         send_buf_size,
                        plw_u32         recv_buf_size,
                        plw_u32         listen_conn_count,
                        plw_drv_tcp_t * ptcp
                        );

/*
 * plw_drv_tcp_delete()
 * @tcp: IN, tcp obj to be deleted.
 */
plw_u32 plw_drv_tcp_delete(plw_drv_tcp_t tcp);


/*
 * plw_drv_tcp_close()
 * @udp: IN, tcp obj
 *
 * this function close the socket and it would cause any block operation such as 
 * plw_drv_tcp_recv() plw_drv_tcp_wait_conn(), and plw_drv_tcp_wait_conn() return 
 * immediately.
 */
plw_u32 plw_drv_tcp_close(plw_drv_tcp_t tcp);


/*
 * plw_drv_tcp_recv()
 * @udp: IN, tcp obj
 * @data_ptr: OUT, buffer to store the data 
 * @bytes_to_recv: IN, bytes to recieve
 * @bytes_received: OUT, the actual bytes received, it can be PLW_NULL
 * @remote_ip: OUT, the ip string of remote host which send the data, it can be PLW_NULL
 * @remote_port: OUT, the port of remoet host which send the data, it can be PLW_NULL
 *
 * this function receives tcp data. If no data available, it would be blocked. If the
 * tcp obj has been closed by plw_drv_tcp_close(), this function return with zero 
 * bytes received (if @bytes_received valid).
 */
plw_u32 plw_drv_tcp_recv
                    (
                    plw_drv_tcp_t   tcp, 
                    void *          data_ptr, 
                    plw_u32         bytes_to_recv, 
                    plw_u32 *       bytes_received,
                    plw_char *      remote_ip, 
                    plw_port_t *    remote_port
                    );

plw_u32 plw_drv_tcp_recv_timeout
                    (
                    plw_drv_tcp_t   tcp, 
                    plw_u32         timeout_ms,
                    void *          data_ptr, 
                    plw_u32         bytes_to_recv, 
                    plw_u32 *       bytes_received,
                    plw_char *      remote_ip, 
                    plw_port_t *    remote_port
                    );
                    


/* plw_drv_tcp_send()
 * @tcp: IN, tcp obj
 * @data_ptr: IN, data to be sent
 * @bytes_to_send: IN, data length
 * @bytes_sent: OUT, data in bytes had been sent, it can be PLW_NULL
 *
 * the connection must be estabished before calling this function.
 */
plw_u32 plw_drv_tcp_send
                    (
                    plw_drv_tcp_t   tcp, 
                    const void *    data_ptr, 
                    const plw_u32   bytes_to_send, 
                    plw_u32 *       bytes_sent
                    );

/*
 * plw_drv_tcp_wait_conn()
 * @tcp_serv: IN, tcp obj
 * @ptcp_conn: OUT, tcp obj for incomming connection
 * @remote_ip: OUT, the ip string of client, it can be PLW_NULL
 * @remote_port: OUT, the port of client, it can be PLW_NULL
 *
 * this function is used for server. it will be block to wait for client connection.
 * when a client connected to server, this function return with a newly create tcp
 * obj store in @ptcp_conn which used for communicate with client.
 */
plw_u32 plw_drv_tcp_wait_conn
                    (
                    plw_drv_tcp_t   tcp_serv, 
                    plw_drv_tcp_t * ptcp_conn, 
                    plw_char *      remote_ip, 
                    plw_port_t *    remote_port
                    );

/*
 * plw_drv_tcp_conn()
 * @tcp: IN, tcp obj
 * @remote_ip: IN, server ip
 * @remote_port: IN, server port
 * @timeout_ms: time-out in milliseconds, set to 0 if using os default time-out
 */
plw_u32 plw_drv_tcp_conn
                    (
                    plw_drv_tcp_t   tcp, 
                    plw_char *      remote_ip, 
                    plw_port_t      remote_port,
                    plw_u32         timeout_ms
                    );

                    
#ifdef __cplusplus  
}
#endif  




#endif



