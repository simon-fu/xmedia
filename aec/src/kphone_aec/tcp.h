/* tcp.cpp
 *
 * Copyright (C) DFS Deutsche Flugsicherung (2004, 2005). 
 * All Rights Reserved.
 *
 * TCP server functions for IPv4
 *
 * Version 0.2
 */

int tcp_server_init(int port);
int tcp_server_init2(int listen_fd);
int socket_async(char *addr, int port);
