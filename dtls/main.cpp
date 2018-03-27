


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gio/gio.h>
#include <glib.h>
#include <agent.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <srtp2/srtp.h>

#include "xdtls_srtp.h"

#include <log4cplus/loggingmacros.h>
#include <log4cplus/logger.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/consoleappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/configurator.h>
// #define dbgv(...) LOG4CPLUS_TRACE_FMT(log4cplus::Logger::getRoot(), __VA_ARGS__)
// #define dbgi(...) LOG4CPLUS_INFO_FMT(log4cplus::Logger::getRoot(), __VA_ARGS__)
// #define dbgw(...) LOG4CPLUS_WARN_FMT(log4cplus::Logger::getRoot(), __VA_ARGS__)
// #define dbge(...) LOG4CPLUS_ERROR_FMT(log4cplus::Logger::getRoot(), __VA_ARGS__)

#define dbgv(...) do{ printf("|%lx|", (long)pthread_self()); printf("<main>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{ printf("|%lx|", (long)pthread_self()); printf("<main>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{ printf("|%lx|", (long)pthread_self()); printf("<main>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)

#define ZERO_ALLOC_(o, type, sz) do{o=(type)malloc(sz); memset(o, 0, sz);}while(0)

#define PORT 5000
#define BLOCK_SIZE 1024



static GMainLoop *gloop ;

typedef struct dtls_conn {
    GSocket *sock;
    GSocketAddress * peer_addr;
    xdtls_srtp_t dtls;
    int is_client;
} dtls_conn;



void _address_to_string (const NiceAddress *addr, gchar *dst)
{
  switch (addr->s.addr.sa_family) {
    case AF_INET:
      inet_ntop (AF_INET, &addr->s.ip4.sin_addr, dst, INET_ADDRSTRLEN);
      break;
    case AF_INET6:
      inet_ntop (AF_INET6, &addr->s.ip6.sin6_addr, dst, INET6_ADDRSTRLEN);
      break;
    default:
      g_return_if_reached ();
  }
}

gboolean xglib_address_to_string (GSocketAddress * gaddr, gchar *dst){
    union {
      struct sockaddr_storage storage;
      struct sockaddr addr;
    } sa;
    NiceAddress naddr;
    g_socket_address_to_native (gaddr, &sa.addr, sizeof (sa), NULL);
    nice_address_set_from_sockaddr (&naddr, &sa.addr);
    nice_address_to_string (&naddr, dst);
    
    sprintf(dst+strlen(dst), ":%d", nice_address_get_port(&naddr));

    return TRUE;
}



static 
gboolean dtls_gio_in (GIOChannel *source, GIOCondition cond, gpointer data)
{
  dtls_conn * conn = (dtls_conn *) data;
  xdtls_srtp_t dtls = conn->dtls;
  GSocket *sock = conn->sock;
  GError *error = NULL;
  gchar buf[1600];
  GSocketAddress * addr = NULL;
  gssize len = g_socket_receive_from (sock,
                       &addr, //GSocketAddress **address,
                       buf,
                       1600,
                       NULL,
                       &error);
  if(len > 0){
    buf[len] = '\0';


    gchar addrstr[INET6_ADDRSTRLEN];
    xglib_address_to_string (addr, addrstr);

    dbgi ("recv from %s, %ld bytes", addrstr, len);

    if(!conn->peer_addr){
      conn->peer_addr = (GSocketAddress *)g_object_ref(addr);
    }

    unsigned char first_byte = buf[0];
    if(first_byte > 127 && first_byte < 192){
      if(xdtls_srtp_is_ready(conn->dtls)){
        int buflen = len;
        buflen = xdtls_srtp_unprotect(conn->dtls, (unsigned char *)buf, buflen);
        if(buflen < 0){
          dbge("xdtls_srtp_unprotect fail, res=%d", buflen);
          return TRUE;
        }
        buf[buflen] = 0;
        dbgi("xdtls_srtp_unprotect len %d, msg=%s", buflen, buf+12);
      }else{
        dbgi("got rtp packet before srtp ready");
      }
      return TRUE;
    }

    int ret = xdtls_srtp_input_data(conn->dtls, (const unsigned char *) buf,  len);
    if(ret == 0){
      if(xdtls_srtp_is_ready(conn->dtls)){
       char sbuf[1024];
       int slen = 0;
       sbuf[0] = 0x80;
       sbuf[1] = 97; // payload type
       sbuf[1] |= 0x80; // M 
       sbuf[2] = 0; sbuf[3] = 0; // seq
       sbuf[4] = sbuf[5] = sbuf[6] = sbuf[7] = 0; // timestamp
       sbuf[8] = sbuf[9] = sbuf[10] = sbuf[11] = 0; // ssrc
       slen = sprintf(sbuf + 12, "Hi from %s", conn->is_client? "client" : "server");
       slen += 12;
       dbgi("rtp len %d", slen);

       slen = xdtls_srtp_protect(conn->dtls, (unsigned char *)sbuf, slen);
       if(slen < 0){
           dbge("xdtls_srtp_protect fail, res=%d", slen);
           return TRUE;
       }
       dbgi("xdtls_srtp_protect len %d", slen);
       
       GError *error = NULL;
       gssize sent_bytes = g_socket_send_to (conn->sock,
                                             conn->peer_addr,
                                             sbuf,
                                             slen,
                                             NULL,
                                             &error);

      }
    }

  }else{
    dbge ("recv error: %ld", len);
  }

  if (addr != NULL){
    g_object_unref (addr);
  }
      
  // g_main_loop_quit(gloop);

  return TRUE;
}


static int on_sending_dtls_data(xdtls_srtp_t dtls,  void * cb_context, const unsigned char * data, int length){
  dtls_conn * conn = (dtls_conn *) cb_context;

       GError *error = NULL;
       gssize sent_bytes = g_socket_send_to (conn->sock,
                                             conn->peer_addr,
                                             (const gchar *)data,
                                             length,
                                             NULL,
                                             &error);
  return sent_bytes;
}



static
gboolean idleCpt (gpointer user_data){

   int *a = (int *)user_data;
   dbgi("%d", *a);
   sleep(1);

   return TRUE;
}

int
main (int argc, char **argv)
{
  log4cplus::BasicConfigurator config;
  config.configure();

  GSocket * s_udp;
  GError *err = NULL;
  int idIdle = -1, dataI = 0;
  guint16 udp_port = PORT;
  guint16 server_port = PORT;
  dtls_conn * conn = NULL;
  int ret = -1;

  int is_client = 0;
  if(argc > 1){
    is_client = atoi(argv[1]);
  }

  dbgi("is_client = %d", is_client);

  ZERO_ALLOC_(conn, dtls_conn * , sizeof(dtls_conn));
  conn->is_client = is_client;

  if(!is_client){
    ret = xdtls_srtp_init("certs/server-cert.pem", "certs/server-key.pem");
    if(ret){
      return 1;
    }
  }else{
    ret = xdtls_srtp_init("certs/client-cert.pem", "certs/client-key.pem");
    if(ret){
      return 1;
    }

  }


  xdtls_role role = XDTLS_ROLE_SERVER;
  if(is_client){
    role = XDTLS_ROLE_CLIENT;
    udp_port++;
  }

  conn->dtls = xdtls_srtp_create(NULL, NULL, role, on_sending_dtls_data, conn);
  if(!conn->dtls){
    return 2;
  }



  GSocketAddress * gsockAddr = G_SOCKET_ADDRESS(g_inet_socket_address_new(g_inet_address_new_any(G_SOCKET_FAMILY_IPV4), udp_port));
  s_udp = g_socket_new(G_SOCKET_FAMILY_IPV4,
                    G_SOCKET_TYPE_DATAGRAM,
                    G_SOCKET_PROTOCOL_UDP,
                    &err);
  g_assert(err == NULL);

  if (s_udp == NULL) {
    dbge("ERREUR");
    exit(1);
  }

  g_socket_set_blocking (s_udp, FALSE);

  if(!is_client){
    if (g_socket_bind (s_udp, gsockAddr, TRUE, NULL) == FALSE){

     dbge("Error bind server\n");
     exit(1);

    }
  }else{
    if (g_socket_bind (s_udp, gsockAddr, FALSE, NULL) == FALSE){

     dbge("Error bind client\n");
     exit(1);

    }
  }


  g_assert(err == NULL);

  int fd = g_socket_get_fd(s_udp);

  GIOChannel* channel = g_io_channel_unix_new(fd);
  // guint source = g_io_add_watch(channel, G_IO_IN,
  //                             (GIOFunc) gio_read_socket, &dataI);

  guint source = 0;
  // if(is_client){
  //   source = g_io_add_watch(channel, G_IO_IN, (GIOFunc) udp_gio_in_client, s_udp);    
  // }else{
  //   source = g_io_add_watch(channel, G_IO_IN, (GIOFunc) udp_gio_in_echo, s_udp);    
  // }  
  source = g_io_add_watch(channel, G_IO_IN, (GIOFunc) dtls_gio_in, conn); 
  

  g_io_channel_unref(channel);
  dbgi("local port %d", udp_port);

  conn->sock = s_udp;

  if(is_client){
    conn->peer_addr = G_SOCKET_ADDRESS(g_inet_socket_address_new_from_string("127.0.0.1", server_port));
    // gssize sent_bytes = g_socket_send_to (s_udp,
    //               dtls->peer_addr,
    //               "123",
    //               4,
    //               NULL,
    //               &err);

  dbgi("start dtls =>");
  xdtls_srtp_start_handshake(conn->dtls);
  dbgi("start dtls <=");
  }


  gloop = g_main_loop_new(NULL, FALSE);
   // idIdle = g_idle_add(idleCpt, &dataI);
  g_main_loop_run(gloop);
  g_message("<%lld> loop stopped", (long long)pthread_self());
  g_source_remove(source);
  g_object_unref(s_udp);
  g_message("<%lld> bye!", (long long)pthread_self());
}

