
/* rpl_sock.h */

#ifndef __RPL_SOCK_H__
#define __RPL_SOCK_H__
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h> 


//#define sock_dbg_info printf
// #define sock_dbg_info(x...) 


typedef int VSOCKET;

#define VSD_BOTH SHUT_RDWR

typedef socklen_t vsocklen_t;
typedef void * vsock_opt_t;

#define verify_sock(xsock)      ((xsock) > 0)
#define verify_sock_ret(xret)   ((xret) != -1 )

#define vclosesock(xsock)   close(xsock)

int vsock_errno(void);

#define vudp_create_after(xsock)

#define vsock_ioctl ioctl

#endif

/* end of file */



