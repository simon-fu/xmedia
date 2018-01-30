

#include "rpl_drv_sock.h"
  
#include "plw_drv.h"
#include "plw_drv_sock.h"

#if 0
//#define sock_dbg_info printf
//#define sock_dbg_info plw_printf
#else
#define  sock_dbg_info(...) 
#endif

static plw_u32 _create_sock(
                        plw_char *      local_ip, 
                        plw_port_t *    pport, 
                        plw_u32         send_buf_size,
                        plw_u32         recv_buf_size,
                        plw_u32         sock_type,
                        plw_u32         proto_type,
                        VSOCKET *       psock
                        )
{
    VSOCKET             sock = 0;
    struct sockaddr_in  servadd;
    vsocklen_t          addr_len = sizeof(servadd);
    plw_u32 ret = PLW_DRVS_SUCCESS;

    int sock_ret;
    int buf_size = 0;
    vsocklen_t opt_len = sizeof(buf_size);
    plw_port_t        port = 0;
    int                 is_multicast = 0;
    unsigned char       net  = 0;

    if(pport)
    {
        port = *pport;
    }
    
    sock = socket(AF_INET, sock_type, proto_type);
    if(!verify_sock(sock))
    {
        // sock_dbg_info("##### socket() error, %d \r\n", vsock_errno());
        sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
        sock = 0;
        goto _create_sock_ret;
    }

    servadd.sin_family = AF_INET;
    servadd.sin_port = htons(port);

    //servadd.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    servadd.sin_addr.s_addr = htonl(INADDR_ANY);
        
    if(local_ip)
    {
        if(strlen(local_ip) > 0)
        {
            //servadd.sin_addr.S_un.S_addr = inet_addr(local_ip);
            servadd.sin_addr.s_addr = inet_addr(local_ip);
            //net = servadd.sin_addr.s_net;  // s_net only defined in windows
            net = (unsigned char) servadd.sin_addr.s_addr & 0xFF;

            if((net >= 224) && (net <= 239))
            {
                is_multicast = 1;
                servadd.sin_addr.s_addr = htonl(INADDR_ANY);
            }
        }
    }




    sock_ret = bind(sock,(const struct sockaddr*)&servadd, sizeof(servadd));
	if( !verify_sock_ret(sock_ret))
	{
	    // sock_dbg_info("##### bind() error, %d\r\n", vsock_errno());
	    sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
		ret = PLW_DRVS_ERROR;
		goto _create_sock_ret;
	}


	getsockname(sock, (struct sockaddr*)&servadd, &addr_len);
	
	if(port == 0 && pport)
	{
       
        *pport = (plw_port_t) ntohs(servadd.sin_port);
	}
	
    if(local_ip)
    {
        if(strlen(local_ip) == 0)
        {
            strcpy(local_ip, inet_ntoa( servadd.sin_addr));
        }
        
    }
        
    // multicast 
    if(is_multicast)
    {
        
        struct ip_mreq remote;
        int len = sizeof(remote);
        remote.imr_multiaddr.s_addr = inet_addr(local_ip);
        remote.imr_interface.s_addr = htonl(INADDR_ANY);

        /*执行了这句就已经加入到了多播组了*/
        sock_ret = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&remote, len);
        if(!verify_sock_ret(sock_ret))
        {
            // sock_dbg_info("setsockopt error, %d\r\n", vsock_errno());
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
            ret = PLW_DRVS_ERROR;
            goto _create_sock_ret;
        }

    }

    /* set recv buff size */
    
    if(recv_buf_size > 0)
	{

        buf_size = (int) recv_buf_size;
        opt_len = sizeof(buf_size);

        sock_ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF ,(vsock_opt_t)&buf_size, opt_len);
        if(!verify_sock_ret(sock_ret))
        {
            // sock_dbg_info("setsockopt error, %d\r\n", vsock_errno());
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
            ret = PLW_DRVS_ERROR;
            goto _create_sock_ret;
        }

        buf_size = 0;
        opt_len = sizeof(buf_size);

        sock_ret = getsockopt(sock, SOL_SOCKET, SO_RCVBUF ,(vsock_opt_t)&buf_size, &opt_len);
        
        if(!verify_sock_ret(sock_ret))
        {
            // sock_dbg_info("getsockopt error, %d\r\n", vsock_errno());
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
            ret = PLW_DRVS_ERROR;
            goto _create_sock_ret;
        }

        if(buf_size < (int)(recv_buf_size))
        {
            // sock_dbg_info("warning: recv_buf_size %u, sock recv buf size %d\r\n", recv_buf_size, buf_size);
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
            ret = PLW_DRVS_ERROR;
            goto _create_sock_ret;
            
        }
    
    }


    /* set send buff size */
    
    if(send_buf_size > 0)
	{

        buf_size = (int) send_buf_size;
        opt_len = sizeof(buf_size);

        sock_ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF ,(vsock_opt_t)&buf_size, opt_len);
        if(!verify_sock_ret(sock_ret))
        {
            // sock_dbg_info("setsockopt SO_SNDBUF error, %d\r\n", vsock_errno());
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
            ret = PLW_DRVS_ERROR;
            goto _create_sock_ret;
        }

        buf_size = 0;
        opt_len = sizeof(buf_size);

        sock_ret = getsockopt(sock, SOL_SOCKET, SO_SNDBUF ,(vsock_opt_t)&buf_size, &opt_len);
        
        if(!verify_sock_ret(sock_ret))
        {
            // sock_dbg_info("getsockopt SO_SNDBUF error, %d\r\n", vsock_errno());
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
            ret = PLW_DRVS_ERROR;
            goto _create_sock_ret;
        }

        if(buf_size < (int)(send_buf_size))
        {
            // sock_dbg_info("warning: send_buf_size %u, sock send buf size %d\r\n", send_buf_size, buf_size);
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
            ret = PLW_DRVS_ERROR;
            goto _create_sock_ret;
        }
    
    }


    
	*psock = sock;

	ret = PLW_DRVS_SUCCESS;

_create_sock_ret:

    if(ret != PLW_DRVS_SUCCESS)
    {

        if(verify_sock(sock))
        {
            vclosesock(sock);
            sock = 0;
        }


    }
    
    return ret;
}


/*
static plw_u32 _close_sock(VSOCKET sock, plw_bool shut)
{
    int sock_ret;

    sock_dbg_info("_close_sock: \r\n");

    if(shut)
    {
        sock_ret = shutdown(sock, VSD_BOTH);
        if(sock_ret != 0)
        {
            sock_dbg_info("_close_sock: shutdown error, %d\r\n", vsock_errno());
        }
    }
    
    vclosesock(sock);
    return 0;
}
*/

static plw_u32 _shutdown_sock(VSOCKET sock)
{
    int sock_ret;

    //sock_dbg_info("_shutdown_sock: \r\n");

    sock_ret = shutdown(sock, VSD_BOTH);
    if(sock_ret != 0)
    {
        //sock_dbg_info("_shutdown_sock: shutdown error, %d\r\n", vsock_errno());
    }

    return 0;
}




struct plw_drv_udp_tag
{
    VSOCKET sock;
    
};

plw_u32 plw_drv_udp_create
                        (
                        plw_char *      local_ip, 
                        plw_port_t *  pport, 
                        plw_u32         send_buf_size,
                        plw_u32         recv_buf_size,
                        plw_drv_udp_t * pudp
                        )
{
    plw_drv_udp_t udp = 0;
    plw_u32 ret = PLW_DRVS_SUCCESS;



    
    udp = (plw_drv_udp_t) malloc( sizeof(struct plw_drv_udp_tag) );

    memset(udp, 0, sizeof(struct plw_drv_udp_tag) );

    ret = _create_sock
                (
                    local_ip, 
                    pport, 
                    send_buf_size,
                    recv_buf_size,
                    SOCK_DGRAM,
                    IPPROTO_UDP,
                    &udp->sock
                );
    if(ret != 0 )
    {
        goto plw_drv_udp_create_ret;
    }

    vudp_create_after(udp->sock);
  
	*pudp = udp;

	ret = PLW_DRVS_SUCCESS;

plw_drv_udp_create_ret:

    if(ret != PLW_DRVS_SUCCESS)
    {
        if(udp)
        {
            if(verify_sock(udp->sock))
            {
                vclosesock(udp->sock);
                udp->sock = 0;
            }
            
            free(udp);
            
            udp = 0;
        }
    }
    
    return ret;
    
}

plw_u32 plw_drv_udp_delete(plw_drv_udp_t udp)
{
    if(verify_sock(udp->sock))
    {
        vclosesock(udp->sock);
        udp->sock = 0;
    }
    
    free(udp);
    
    udp = 0;
            
    return 0;
    
}


plw_u32 plw_drv_udp_close(plw_drv_udp_t udp)
{
    if(udp->sock)
    {
        _shutdown_sock(udp->sock);
        vclosesock(udp->sock);
        udp->sock = 0;
    }
    
    return 0;
}


plw_u32 plw_drv_udp_recv
                    (
                    plw_drv_udp_t   udp, 
                    void *          data_ptr, 
                    plw_u32         bytes_to_recv, 
                    plw_u32 *       bytes_received,
                    plw_char *      remote_ip, 
                    plw_port_t *  remote_port
                    )
{
    plw_u32 ret = PLW_DRVS_SUCCESS;
    struct sockaddr_in tempadd;
    int sock_ret;
    vsocklen_t len = 0;
    plw_u32 bytes = 0;
    char *buff = (char *)data_ptr;

   
    
    len = sizeof(tempadd);
    
    sock_ret = recvfrom(udp->sock, buff, bytes_to_recv, 0, (struct sockaddr*)&tempadd, &len);
    
    if(!verify_sock_ret(sock_ret))
    {
        //sock_dbg_info("plw_drv_udp_recv: recvfrom error, %d\r\n", vsock_errno());
        ret = PLW_DRVS_ERROR;
        bytes = 0;
    }
    else
    {
        if(remote_ip)
        {
            strcpy(remote_ip, inet_ntoa( tempadd.sin_addr));
            
        }

        if(remote_port)
        {
            *remote_port = (plw_port_t) ntohs(tempadd.sin_port);
        }
        
        bytes = sock_ret;
    }

    if(bytes_received)
    {
        *bytes_received = bytes;
    }
    

    
    return ret;
}


plw_u32 plw_drv_udp_send
                    (
                    plw_drv_udp_t   udp, 
                    const plw_char *remote_ip, 
                    plw_port_t      remote_port,
                    void *          data_ptr, 
                    plw_u32         bytes_to_send, 
                    plw_u32 *       bytes_sent
                    )
{
    struct sockaddr_in  addrto;
    vsocklen_t          tolen = 0;
    // char                opt = 1;
    char *              buff = (char *)data_ptr;
    int                 sock_ret;
    plw_u32             bytes = 0;
    plw_u32             ret = PLW_DRVS_SUCCESS;

    addrto.sin_family=AF_INET;
    // addrto.sin_addr.s_addr = INADDR_BROADCAST;
    addrto.sin_addr.s_addr = inet_addr(remote_ip);
    addrto.sin_port = htons(remote_port);
    tolen = sizeof(addrto);

    /*
    if(strcmp(remote_ip, "255.255.255.255") == 0)
    {
        sock_dbg_info("plw_drv_udp_send: broadcast ip\r\n");
	    setsockopt(udp->sock, SOL_SOCKET, SO_BROADCAST,(vsock_opt_t)&opt,sizeof(opt));
	}
	*/

	sock_ret = sendto(udp->sock, buff, bytes_to_send, 0, (struct sockaddr*)&addrto, tolen);


	if(!verify_sock_ret(sock_ret))
	{
        // sock_dbg_info("plw_drv_udp_send: send error, %d\r\n", vsock_errno());
        sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
        ret = PLW_DRVS_ERROR;
        bytes = 0;
	}
	else
	{
        bytes = sock_ret;
	}
	

	if(bytes_sent)
	{
        *bytes_sent = bytes;
	}
	
    return ret;
}


plw_u32 plw_drv_udp_recv_timeout
                    (
                    plw_drv_udp_t   udp, 
                    plw_u32         timeout_ms,
                    void *          data_ptr, 
                    plw_u32         bytes_to_recv, 
                    plw_u32 *       bytes_received,
                    plw_char *      remote_ip, 
                    plw_port_t *    remote_port
                    )
{

    struct timeval     timeout   ;   
    fd_set              r;
    int                 last_err;
    int                 sock_ret = 0;
    
    FD_ZERO(&r);   
    FD_SET(udp->sock,   &r);   
    timeout.tv_sec   =   timeout_ms / 1000;
    timeout.tv_usec  =   (timeout_ms % 1000) * 1000;

    sock_ret   =   select((int)(udp->sock) + 1,   &r,   0,   0,   &timeout);
    
    if(!verify_sock_ret(sock_ret))  
    {   
        last_err = vsock_errno();
        //sock_dbg_info( "plw_drv_udp_recv_timeout: select fail, err %d \n", last_err);
        sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, last_err);
        return PLW_DRVS_ERROR;
    }
    else if(sock_ret == 0)
    {
        // sock_dbg_info("plw_drv_udp_recv_timeout: select timeout\r\n");
        sock_dbg_info("@@@@ wrn on %s, %u\r\n",  __FILE__, __LINE__);
        return  PLW_DRVS_TIMEOUT;
    }

    return plw_drv_udp_recv
                (
                udp, 
                data_ptr, 
                bytes_to_recv, 
                bytes_received,
                remote_ip, 
                remote_port
                );

}


plw_u32 plw_drv_udp_broadcast
                    (
                    plw_drv_udp_t       udp, 
                    plw_char *    remote_ip, 
                    plw_port_t          remote_port,
                    void *              data_ptr, 
                    plw_u32             bytes_to_send, 
                    plw_u32 *           bytes_sent
                    )
{
    int                opt = 1;
    
    setsockopt(udp->sock, SOL_SOCKET, SO_BROADCAST,(vsock_opt_t)&opt,sizeof(opt));
    
    return plw_drv_udp_send
                    (
                    udp, 
                    remote_ip, 
                    remote_port,
                    data_ptr, 
                    bytes_to_send, 
                    bytes_sent
                    );


}










struct plw_drv_tcp_tag
{
    VSOCKET         sock;
    plw_u32         listen_conn_count;
};



plw_u32 plw_drv_tcp_create
                        (
                        plw_char *      local_ip, 
                        plw_port_t *    pport, 
                        plw_u32         send_buf_size,
                        plw_u32         recv_buf_size,
                        plw_u32         listen_conn_count,
                        plw_drv_tcp_t * ptcp
                        )

{
    plw_drv_tcp_t tcp = 0;
    plw_u32 ret = PLW_DRVS_SUCCESS;
    int     sock_ret;

    
    tcp = (plw_drv_tcp_t)malloc( sizeof(struct plw_drv_tcp_tag) );

    memset(tcp, 0, sizeof(struct plw_drv_tcp_tag) );

    ret = _create_sock
                (
                    local_ip, 
                    pport, 
                    send_buf_size,
                    recv_buf_size,
                    SOCK_STREAM,
                    IPPROTO_TCP,
                    &tcp->sock
                );
    if(ret != 0 )
    {
        // sock_dbg_info("plw_drv_tcp_create: _create_sock error \r\n");
        sock_dbg_info("@@@@ wrn on %s, %u\r\n",  __FILE__, __LINE__);
        goto plw_drv_tcp_create_ret;
    }

    tcp->listen_conn_count = listen_conn_count;

    if(listen_conn_count > 0)
    {
        /* listen */    
        
        sock_ret = listen(tcp->sock, SOMAXCONN);
        
        if(!verify_sock_ret(sock_ret))
        {
            // sock_dbg_info("plw_drv_tcp_create: listen error, %d\r\n", vsock_errno());
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
            ret = PLW_DRVS_ERROR;
            goto plw_drv_tcp_create_ret;
        }

    }

	*ptcp = tcp;

	ret = PLW_DRVS_SUCCESS;

plw_drv_tcp_create_ret:

    if(ret != PLW_DRVS_SUCCESS)
    {
        if(tcp)
        {
            if(verify_sock(tcp->sock))
            {
                vclosesock(tcp->sock);
                tcp->sock = 0;
            }
            
            free(tcp);
            
            tcp = 0;
        }
    }
    
    return ret;
}



plw_u32 plw_drv_tcp_delete(plw_drv_tcp_t tcp)
{
    if(verify_sock(tcp->sock))
    {
        vclosesock(tcp->sock);
        tcp->sock = 0;
    }
    
    free(tcp);
    
    tcp = 0;
            
    return 0;
    
}

plw_u32 plw_drv_tcp_close(plw_drv_tcp_t tcp)
{
    if(verify_sock(tcp->sock))
    {
       // if(tcp->listen_conn_count == 0)
        {
            _shutdown_sock(tcp->sock);
        }

            vclosesock(tcp->sock);
            tcp->sock = 0;


    }
    
    return 0;
}


plw_u32 plw_drv_tcp_recv
                    (
                    plw_drv_tcp_t   tcp, 
                    void *          data_ptr, 
                    plw_u32         bytes_to_recv, 
                    plw_u32 *       bytes_received,
                    plw_char *      remote_ip, 
                    plw_port_t *  remote_port
                    )
{
    plw_u32                 ret = PLW_DRVS_SUCCESS;
    struct sockaddr_in      tempadd;
    int                     sock_ret;
    vsocklen_t              len = 0;
    plw_u32                 bytes = 0;
    char *                  buff = (char *)data_ptr;
    int                     last_err;

   
    
    len = sizeof(tempadd);
    
    sock_ret = recvfrom(tcp->sock, buff, bytes_to_recv, 0, (struct sockaddr*)&tempadd, &len);
    
    
    if(!verify_sock_ret(sock_ret))
    {
        last_err = vsock_errno();
        /*
        if(last_err == 10058)
        {
            // sock_dbg_info("plw_drv_tcp_recv: shutdown\r\n");
            sock_dbg_info("@@@@ wrn on %s, %u\r\n",  __FILE__, __LINE__);
            ret = PLW_DRVS_SUCCESS;
        }
        */
            // sock_dbg_info("plw_drv_tcp_recv: recvfrom error, %d\r\n", last_err);
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, last_err);
            ret = PLW_DRVS_ERROR;

        
        bytes = 0;
    }
    else
    {
        if(remote_ip)
        {
            strcpy(remote_ip, inet_ntoa( tempadd.sin_addr));
            
        }

        if(remote_port)
        {
            *remote_port = (plw_port_t) ntohs(tempadd.sin_port);
        }
        
        bytes = sock_ret;
    }

    if(bytes_received)
    {
        *bytes_received = bytes;
    }
    

    
    return ret;
}


plw_u32 plw_drv_tcp_recv_timeout
                    (
                    plw_drv_tcp_t   tcp, 
                    plw_u32         timeout_ms,
                    void *          data_ptr, 
                    plw_u32         bytes_to_recv, 
                    plw_u32 *       bytes_received,
                    plw_char *      remote_ip, 
                    plw_port_t *    remote_port
                    )
{
    struct timeval     timeout   ;   
    fd_set              r;
    int                 last_err;
    int                 sock_ret = 0;
    
    FD_ZERO(&r);   
    FD_SET(tcp->sock,   &r);   
    timeout.tv_sec   =   timeout_ms / 1000;
    timeout.tv_usec  =   (timeout_ms % 1000) * 1000;

    sock_ret   =   select((int)(tcp->sock) + 1,   &r,   0,   0,   &timeout);
    
    if(!verify_sock_ret(sock_ret))  
    {   
        last_err = vsock_errno();
        sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, last_err);
        return PLW_DRVS_ERROR;
    }
    else if(sock_ret == 0)
    {
        sock_dbg_info("@@@@ wrn on %s, %u\r\n",  __FILE__, __LINE__);
        return  PLW_DRVS_TIMEOUT;
    }

    return plw_drv_tcp_recv
                    (
                    tcp, 
                    data_ptr, 
                    bytes_to_recv, 
                    bytes_received,
                    remote_ip, 
                    remote_port
                    );
}

plw_u32 plw_drv_tcp_send
                    (
                    plw_drv_tcp_t   tcp, 
                    const void *    data_ptr, 
                    const plw_u32   bytes_to_send, 
                    plw_u32 *       bytes_sent
                    )
{
    plw_u32             ret = PLW_DRVS_SUCCESS;

    char *              buff = (char *)data_ptr;
    int                 sock_ret;
    plw_u32             bytes = 0;



	sock_ret = send(tcp->sock, buff, bytes_to_send, 0);

	if(!verify_sock_ret(sock_ret))
	{
        // sock_dbg_info("plw_drv_tcp_send: send error, %d\r\n", vsock_errno());
        sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
        ret = PLW_DRVS_ERROR;
        bytes = 0;
	}
	else
	{
        bytes = sock_ret;
	}

	if(bytes_sent)
	{
        *bytes_sent = bytes;
	}
	
    return ret;
}


/*

wait for connection 

*/
plw_u32 plw_drv_tcp_wait_conn
                    (
                    plw_drv_tcp_t   tcp_serv, 
                    plw_drv_tcp_t * ptcp_conn, 
                    plw_char *      remote_ip, 
                    plw_port_t *  remote_port
                    )
{
    plw_u32                 ret = 0;

    plw_drv_tcp_t           tcp_conn = 0;
    struct sockaddr_in      tempadd;
    vsocklen_t              len = 0;
    int                     last_err;

    *ptcp_conn = 0;

    tcp_conn = (plw_drv_tcp_t) malloc( sizeof(struct plw_drv_tcp_tag) );

    memset(tcp_conn, 0, sizeof(struct plw_drv_tcp_tag) );
    tcp_conn->listen_conn_count = 0;
    
    /* accept */

    len = sizeof(tempadd);
    
    tcp_conn->sock = accept( tcp_serv->sock, (struct sockaddr*)&tempadd,&len );
    
    if(!verify_sock(tcp_conn->sock))
    {
        last_err = vsock_errno();
        

            // sock_dbg_info("plw_drv_tcp_wait_conn: accept error, %d\r\n", last_err);
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, last_err);
            tcp_conn->sock = 0;
            ret = PLW_DRVS_ERROR;
            goto plw_drv_tcp_wait_conn_ret;

        /*
        if(last_err == 10004)
        {
            tcp_conn->sock = 0;
            plw_drv_tcp_delete(tcp_conn);
            tcp_conn = 0;
        }
        */
    }
    else
    {
        
        
        if(remote_ip)
        {
            strcpy(remote_ip, inet_ntoa( tempadd.sin_addr));
            
        }

        if(remote_port)
        {
            *remote_port = (plw_port_t) ntohs(tempadd.sin_port);
        }
    }



    *ptcp_conn = tcp_conn;
    
    ret = PLW_DRVS_SUCCESS;

    

plw_drv_tcp_wait_conn_ret:

    if(ret != 0)
    {
        if(tcp_conn)
        {
            plw_drv_tcp_delete(tcp_conn);
            tcp_conn = 0;
        }
    }

    
    return ret;
}

#if 0
plw_u32 plw_drv_tcp_conn
                    (
                    plw_drv_tcp_t   tcp, 
                    plw_char *      remote_ip, 
                    plw_port_t    remote_port
                    )
{


    struct sockaddr_in      clientService; 
    int                     last_err;
    
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr( remote_ip );
    clientService.sin_port = htons( remote_port );

    if ( connect( tcp->sock, (SOCKADDR*) &clientService, sizeof(clientService) ) == SOCKET_ERROR) 
    {
        last_err = vsock_errno();
        sock_dbg_info( "plw_drv_tcp_conn: failed to connect %s:%u, err %d \n", remote_ip, remote_port, last_err);
        return PLW_DRVS_ERROR;
    }

    return PLW_DRVS_SUCCESS;

}
#endif

plw_u32 plw_drv_tcp_conn
                    (
                    plw_drv_tcp_t   tcp, 
                    plw_char *      remote_ip, 
                    plw_port_t      remote_port,
                    plw_u32         timeout_ms
                    )
{

    struct sockaddr_in      clientService; 
    unsigned   long         ul   =   1;
    int sock_ret = 0;
    struct   timeval        timeout   ;   
    fd_set                  r;   
    int                     last_err;
    plw_u32                 vret = PLW_DRVS_SUCCESS;
    
    int                     is_error = 0;
    vsocklen_t              len = 0;
  
    /* set to non-block mode */

    if(timeout_ms > 0)
    {
        ul = 1;
        sock_ret   =   vsock_ioctl(tcp->sock,   FIONBIO,   (unsigned   long*)&ul);   
        if(!verify_sock_ret(sock_ret))
        {
            last_err = vsock_errno();
            // sock_dbg_info( "plw_drv_tcp_conn_timeout: failed to ioctlsocket non-block mode, err %d, %s:%u \n", last_err, remote_ip, remote_port);
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, last_err);
            
            return   PLW_DRVS_ERROR;  
        }
    }


    /* conncet to remote */
    
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr( remote_ip );
    clientService.sin_port = htons( remote_port );

    sock_ret = connect( tcp->sock, (struct sockaddr *) &clientService, sizeof(clientService) );
    if(verify_sock_ret(sock_ret))
    {
        // sock_dbg_info("plw_drv_tcp_conn_timeout:connect success directly\r\n");
        sock_dbg_info("@@@@ wrn on %s, %u\r\n",  __FILE__, __LINE__);
        vret = PLW_DRVS_SUCCESS;
        
    }
    else
    {
    
        if(timeout_ms == 0)
        {
            // sock_dbg_info("plw_drv_tcp_conn_timeout:timeout_ms 0, return failure directly\r\n");
            sock_dbg_info("@@@@ wrn on %s, %u\r\n",  __FILE__, __LINE__);
            return PLW_DRVS_ERROR;
        }
        
        #if 0
        last_err = vsock_errno();
        if(!(timeout_ms > 0 && last_err == WSAEWOULDBLOCK)) /* linux is EWOULDBLOCK */
        {
            sock_dbg_info( "plw_drv_tcp_conn_timeout: failed to connect %s:%u, err %d \n", remote_ip, remote_port, last_err);
            return PLW_DRVS_ERROR;
        }
        #endif
        



        /* wait for connecting complete */
        
        FD_ZERO(&r);   
        FD_SET(tcp->sock,   &r);   
        timeout.tv_sec   =   timeout_ms / 1000;
        timeout.tv_usec  =   (timeout_ms % 1000) * 1000;

        sock_ret   =   select((int)(tcp->sock) + 1,   0,   &r,   0,   &timeout);
        
        if(!verify_sock_ret(sock_ret))  
        {   
            last_err = vsock_errno();
            // sock_dbg_info( "plw_drv_tcp_conn_timeout: select fail, err %d \n", last_err);
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, last_err);
            return PLW_DRVS_ERROR;
        }
        else if(sock_ret == 0)
        {
            // sock_dbg_info("plw_drv_tcp_conn_timeout: select timeout\r\n");
            sock_dbg_info("@@@@ wrn on %s, %u\r\n",  __FILE__, __LINE__);
            vret = PLW_DRVS_TIMEOUT;
        }
        else
        {
            // sock_dbg_info("plw_drv_tcp_conn_timeout: select return > 0\r\n");
            //sock_dbg_info("@@@@ wrn on %s, %u\r\n",  __FILE__, __LINE__);
            
            len = sizeof(is_error);
            sock_ret = getsockopt(tcp->sock, SOL_SOCKET, SO_ERROR, (vsock_opt_t) &is_error, (vsocklen_t *)&len);
            if(!verify_sock_ret(sock_ret))
            {
                // sock_dbg_info("plw_drv_tcp_conn: getsockopt SO_SNDBUF error, %d\r\n", vsock_errno());
                sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, vsock_errno());
                vret = PLW_DRVS_ERROR;
                return vret;
            }

            // sock_dbg_info("plw_drv_tcp_conn_timeout: is_error %d\r\n", is_error);
            
            if(is_error == 0) 
            {
                vret = PLW_DRVS_SUCCESS;
            }
            else 
            {
                vret = PLW_DRVS_TIMEOUT;
            }

        }
        
    }

    /* set back to block mode*/

    if(timeout_ms > 0)
    {
        ul = 0;
        sock_ret   =   vsock_ioctl(tcp->sock,   FIONBIO,   (unsigned   long*)&ul);   
        if(!verify_sock_ret(sock_ret))
        {   
            last_err = vsock_errno();
            // sock_dbg_info( "plw_drv_tcp_conn_timeout: ioctlsocket block mode fail, err %d \n", last_err);
            sock_dbg_info("@@@@ wrn on %s, %u, err %d\r\n",  __FILE__, __LINE__, last_err);
            return PLW_DRVS_ERROR;
        }
    }


    return vret;
    
}


#if 0

int t(void)
{

  WSADATA   wsd;   
  VSOCKET   cClient;   
  int   ret;   
  struct   sockaddr_in   server;   
  struct hostent   *host=NULL;   
  int   TimeOut=6000;   //设置发送超时6秒       
  
  if(WSAStartup(MAKEWORD(2,0),&wsd)){return   0;}   
  cClient=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);   
  if(cClient==INVALID_SOCKET){return   0;}   
  //set   Recv   and   Send   time   out   

  if(::setsockopt(cClient,SOL_SOCKET,SO_SNDTIMEO,(char   *)&TimeOut,sizeof(TimeOut))==SOCKET_ERROR){   
  return   0;   
  }   
  TimeOut=6000;//设置接收超时6秒   
  if(::setsockopt(cClient,SOL_SOCKET,SO_RCVTIMEO,(char   *)&TimeOut,sizeof(TimeOut))==SOCKET_ERROR){   
  return   0;   
  }   
  //设置非阻塞方式连接   
  unsigned   long   ul   =   1;   
  ret   =   ioctlsocket(cClient,   FIONBIO,   (unsigned   long*)&ul);   
  if(ret==SOCKET_ERROR)return   0;   
    
  //连接   
  server.sin_family   =   AF_INET;   
  server.sin_port   =   htons(25);   
  server.sin_addr   .s_addr   =   inet_addr((LPCSTR)pSmtp);   
  if(server.sin_addr.s_addr   ==   INADDR_NONE){return   0;}   
    
  connect(cClient,(const   struct   sockaddr   *)&server,sizeof(server));   
    
  //select   模型，即设置超时   
  struct   timeval   timeout   ;   
  fd_set   r;   
    
  FD_ZERO(&r);   
  FD_SET(cClient,   &r);   
  timeout.tv_sec   =   15;   //连接超时15秒   
  timeout.tv_usec   =0;   
  ret   =   select(0,   0,   &r,   0,   &timeout);   
  if   (   ret   <=   0   )   
  {   
  ::closesocket(cClient);   
  return   0;   
  }   
  //一般非锁定模式套接比较难控制，可以根据实际情况考虑   再设回阻塞模式   
  unsigned   long   ul1=   0   ;   
  ret   =   ioctlsocket(cClient,   FIONBIO,   (unsigned   long*)&ul1);   
  if(ret==SOCKET_ERROR){   
  ::closesocket   (cClient);   
  return   0;   
  }   

}

#endif



/* end of file */



