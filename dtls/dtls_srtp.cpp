


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



#define PORT 5000
#define BLOCK_SIZE 1024



static GMainLoop *gloop ;

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
gboolean udp_gio_in_echo (GIOChannel *source, GIOCondition cond, gpointer data)
{
  // g_print("udp_gio_in_echo\n");
  GSocket *sock = (GSocket *)data;
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

    g_print ("recv from %s, %ld bytes\n", addrstr, len);

    gssize sent_bytes = g_socket_send_to (sock,
                  addr,
                  buf,
                  len,
                  NULL,
                  &error);

  }else{
    g_print ("recv error: %ld\n", len);
  }

  if (addr != NULL){
    g_object_unref (addr);
  }
      
  // g_main_loop_quit(gloop);

  return TRUE;
}

static 
gboolean udp_gio_in_client (GIOChannel *source, GIOCondition cond, gpointer data)
{
  // g_print("udp_gio_in_echo\n");
  GSocket *sock = (GSocket *)data;
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

    g_print ("recv from %s, %ld bytes\n", addrstr, len);

  }else{
    g_print ("recv error: %ld\n", len);
  }

  if (addr != NULL){
    g_object_unref (addr);
  }
      
  // g_main_loop_quit(gloop);

  return TRUE;
}





typedef pthread_mutex_t xdtls_mutex;
static int lock_debug = 0;
#define xdtls_mutex_init(a) pthread_mutex_init(a,NULL)
#define xdtls_mutex_destroy(a) pthread_mutex_destroy(a)
/*! \brief Janus mutex lock without debug */
#define xdtls_mutex_lock_nodebug(a) pthread_mutex_lock(a);
/*! \brief Janus mutex lock with debug (prints the line that locked a mutex) */
#define xdtls_mutex_lock_debug(a) { printf("[%s:%s:%d:] ", __FILE__, __FUNCTION__, __LINE__); printf("LOCK %p\n", a); pthread_mutex_lock(a); };
/*! \brief Janus mutex lock wrapper (selective locking debug) */
#define xdtls_mutex_lock(a) { if(!lock_debug) { xdtls_mutex_lock_nodebug(a); } else { xdtls_mutex_lock_debug(a); } };
/*! \brief Janus mutex unlock without debug */
#define xdtls_mutex_unlock_nodebug(a) pthread_mutex_unlock(a);
/*! \brief Janus mutex unlock with debug (prints the line that unlocked a mutex) */
#define xdtls_mutex_unlock_debug(a) { printf("[%s:%s:%d:] ", __FILE__, __FUNCTION__, __LINE__); printf("UNLOCK %p\n", a); pthread_mutex_unlock(a); };
#define xdtls_mutex_unlock(a) { if(!lock_debug) { xdtls_mutex_unlock_nodebug(a); } else { xdtls_mutex_unlock_debug(a); } };

typedef struct udpx_st* udpx_t;

struct udpx_st{
  GSocket *sock;

};

#define DTLS_CIPHERS  "ALL:NULL:eNULL:aNULL"
/* SRTP stuff (http://tools.ietf.org/html/rfc3711) */
#define SRTP_MASTER_KEY_LENGTH  16
#define SRTP_MASTER_SALT_LENGTH 14
#define SRTP_MASTER_LENGTH (SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_SALT_LENGTH)


static xdtls_mutex * g_dtls_locks = NULL;;
static SSL_CTX * ssl_ctx = NULL;
static gchar local_fingerprint[160];

SSL_CTX *xdtls_get_ssl_ctx(void) {
  return ssl_ctx;
}


static void xdtls_cb_openssl_threadid(CRYPTO_THREADID *tid) {
  /* FIXME Assuming pthread, which is fine as GLib wraps pthread and
   * so that's what we use anyway? */
  pthread_t me = pthread_self();

  if(sizeof(me) == sizeof(void *)) {
    CRYPTO_THREADID_set_pointer(tid, (void *) me);
  } else {
    CRYPTO_THREADID_set_numeric(tid, (unsigned long) me);
  }
}

static void xdtls_cb_openssl_lock(int mode, int type, const char *file, int line) {
  if((mode & CRYPTO_LOCK)) {
    xdtls_mutex_lock(&g_dtls_locks[type]);
  } else {
    xdtls_mutex_unlock(&g_dtls_locks[type]);
  }
}

/* DTLS certificate verification callback */
static
int xdtls_verify_callback(int preverify_ok, X509_STORE_CTX *ctx) {
  /* We just use the verify_callback to request a certificate from the client */
  dbgi("xdtls_verify_callback: preverify_ok=%d, x509ctx=%p", preverify_ok, ctx);
  return 1;
}

gint xdtls_srtp_init(const gchar *server_pem, const gchar *server_key) {
  /* FIXME First of all make OpenSSL thread safe (see note above on issue #316) */
  g_dtls_locks = (xdtls_mutex *) g_malloc0(sizeof(*g_dtls_locks) * CRYPTO_num_locks());
  int l=0;
  for(l = 0; l < CRYPTO_num_locks(); l++) {
    xdtls_mutex_init(&g_dtls_locks[l]);
  }

  CRYPTO_THREADID_set_callback(xdtls_cb_openssl_threadid);
  CRYPTO_set_locking_callback(xdtls_cb_openssl_lock);

  OpenSSL_add_ssl_algorithms();
  SSL_load_error_strings();

  /* Go on and create the DTLS context */
  ssl_ctx = SSL_CTX_new(DTLSv1_method());
  // ssl_ctx = SSL_CTX_new(DTLSv1_server_method());
  if(!ssl_ctx) {
    dbge("Ops, error creating DTLS context?\n");
    return -1;
  }
  SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, xdtls_verify_callback);
  SSL_CTX_set_tlsext_use_srtp(ssl_ctx, "SRTP_AES128_CM_SHA1_80"); /* FIXME Should we support something else as well? */
  if(!server_pem || !SSL_CTX_use_certificate_file(ssl_ctx, server_pem, SSL_FILETYPE_PEM)) {
    dbge("Certificate error (%s)\n", ERR_reason_error_string(ERR_get_error()));
    return -2;
  }
  if(!server_key || !SSL_CTX_use_PrivateKey_file(ssl_ctx, server_key, SSL_FILETYPE_PEM)) {
    dbge("Certificate key error (%s)\n", ERR_reason_error_string(ERR_get_error()));
    return -3;
  }
  if(!SSL_CTX_check_private_key(ssl_ctx)) {
    dbge("Certificate check error (%s)\n", ERR_reason_error_string(ERR_get_error()));
    return -4;
  }
  SSL_CTX_set_read_ahead(ssl_ctx,1);
  BIO *certbio = BIO_new(BIO_s_file());
  if(certbio == NULL) {
    dbge( "Certificate BIO error...\n");
    return -5;
  }
  if(BIO_read_filename(certbio, server_pem) == 0) {
    dbge("Error reading certificate (%s)\n", ERR_reason_error_string(ERR_get_error()));
    BIO_free_all(certbio);
    return -6;
  }
  X509 *cert = PEM_read_bio_X509(certbio, NULL, 0, NULL);
  if(cert == NULL) {
    dbge("Error reading certificate (%s)\n", ERR_reason_error_string(ERR_get_error()));
    BIO_free_all(certbio);
    return -7;
  }
  unsigned int size;
  unsigned char fingerprint[EVP_MAX_MD_SIZE];
  if(X509_digest(cert, EVP_sha256(), (unsigned char *)fingerprint, &size) == 0) {
    dbge( "Error converting X509 structure (%s)\n", ERR_reason_error_string(ERR_get_error()));
    X509_free(cert);
    BIO_free_all(certbio);
    return -7;
  }
  char *lfp = (char *)&local_fingerprint;
  unsigned int i = 0;
  for(i = 0; i < size; i++) {
    g_snprintf(lfp, 4, "%.2X:", fingerprint[i]);
    lfp += 3;
  }
  *(lfp-1) = 0;
  dbgi("Fingerprint of our certificate: %s\n", local_fingerprint);
  X509_free(cert);
  BIO_free_all(certbio);
  SSL_CTX_set_cipher_list(ssl_ctx, DTLS_CIPHERS);

  /* Initialize libsrtp */
  if(srtp_init() != srtp_err_status_ok) {
    dbge("Ops, error setting up libsrtp?\n");
    return 5;
  }
  return 0;
}


/*! \brief DTLS roles */
typedef enum janus_dtls_role {
  JANUS_DTLS_ROLE_ACTPASS = -1,
  JANUS_DTLS_ROLE_SERVER,
  JANUS_DTLS_ROLE_CLIENT,
} janus_dtls_role;

/*! \brief DTLS state */
typedef enum janus_dtls_state {
  JANUS_DTLS_STATE_FAILED = -1,
  JANUS_DTLS_STATE_CREATED,
  JANUS_DTLS_STATE_TRYING,
  JANUS_DTLS_STATE_CONNECTED,
} janus_dtls_state;


typedef struct xdtls_srtp {
  /*! \brief Opaque pointer to the component this DTLS-SRTP context belongs to */
  void *component;
  /*! \brief DTLS role of the gateway for this stream: 1=client, 0=server */
  janus_dtls_role dtls_role;
  /*! \brief DTLS state of this component: -1=failed, 0=nothing, 1=trying, 2=connected */
  janus_dtls_state dtls_state;
  /*! \brief Monotonic time of when the DTLS state has switched to connected */
  gint64 dtls_connected;
  /*! \brief SSL context used for DTLS for this component */
  SSL *ssl;
  /*! \brief Read BIO (incoming DTLS data) */
  BIO *read_bio;
  /*! \brief Write BIO (outgoing DTLS data) */
  BIO *write_bio;
  /*! \brief Filter BIO (fix MTU fragmentation on outgoing DTLS data, if required) */
  BIO *filter_bio;
  /*! \brief Whether SRTP has been correctly set up for this component or not */
  gint srtp_valid;
  /*! \brief libsrtp context for incoming SRTP packets */
  srtp_t srtp_in;
  /*! \brief libsrtp context for outgoing SRTP packets */
  srtp_t srtp_out;
  /*! \brief libsrtp policy for incoming SRTP packets */
  srtp_policy_t remote_policy;
  /*! \brief libsrtp policy for outgoing SRTP packets */
  srtp_policy_t local_policy;
  /*! \brief Mutex to lock/unlock this libsrtp context */
  xdtls_mutex srtp_mutex;
  /*! \brief Whether this DTLS stack is now ready to be used for messages as well (e.g., SCTP encapsulation) */
  int ready;


  GSocket *sock;
  GSocketAddress * peer_addr;
  int handshake_done;

} xdtls_srtp;





/* Starting MTU value for the DTLS BIO filter */
static int mtu = 1472;
void janus_dtls_bio_filter_set_mtu(int start_mtu) {
  if(start_mtu < 0) {
    dbge("Invalid MTU...\n");
    return;
  }
  mtu = start_mtu;
  dbgi("Setting starting MTU in the DTLS BIO filter: %d\n", mtu);
}

/* Filter implementation */
int janus_dtls_bio_filter_write(BIO *h, const char *buf,int num);
long janus_dtls_bio_filter_ctrl(BIO *h, int cmd, long arg1, void *arg2);
int janus_dtls_bio_filter_new(BIO *h);
int janus_dtls_bio_filter_free(BIO *data);

static BIO_METHOD janus_dtls_bio_filter_methods = {
  BIO_TYPE_FILTER,
  "janus filter",
  janus_dtls_bio_filter_write,
  NULL,
  NULL,
  NULL,
  janus_dtls_bio_filter_ctrl,
  janus_dtls_bio_filter_new,
  janus_dtls_bio_filter_free,
  NULL
};
BIO_METHOD *BIO_janus_dtls_filter(void) {
  return(&janus_dtls_bio_filter_methods);
}


/* Helper struct to keep the filter state */
typedef struct janus_dtls_bio_filter {
  GList *packets;
  xdtls_mutex mutex;
  xdtls_srtp * dtls;
} janus_dtls_bio_filter;


int janus_dtls_bio_filter_new(BIO *bio) {
  /* Create a filter state struct */
  janus_dtls_bio_filter *filter = (janus_dtls_bio_filter *) g_malloc0(sizeof(janus_dtls_bio_filter));
  filter->packets = NULL;
  xdtls_mutex_init(&filter->mutex);
  
  /* Set the BIO as initialized */
  bio->init = 1;
  bio->ptr = filter;
  bio->flags = 0;
  
  return 1;
}

int janus_dtls_bio_filter_free(BIO *bio) {
  if(bio == NULL)
    return 0;
    
  /* Get rid of the filter state */
  janus_dtls_bio_filter *filter = (janus_dtls_bio_filter *)bio->ptr;
  if(filter != NULL) {
    g_list_free(filter->packets);
    filter->packets = NULL;
    g_free(filter);
  }
  bio->ptr = NULL;
  bio->init = 0;
  bio->flags = 0;
  return 1;
}
  
int janus_dtls_bio_filter_write(BIO *bio, const char *in, int inl) {
  
  janus_dtls_bio_filter *filter = (janus_dtls_bio_filter *)bio->ptr;
  if(filter->dtls->peer_addr){
    dbgi("janus_dtls_bio_filter_write: send %p, %d\n", in, inl);
    GError *error = NULL;
    gssize sent_bytes = g_socket_send_to (filter->dtls->sock,
                  filter->dtls->peer_addr,
                  in,
                  inl,
                  NULL,
                  &error);
  }else{
    dbgi("janus_dtls_bio_filter_write: skip %p, %d\n", in, inl);
  }
  return inl;

  // dbgi("janus_dtls_bio_filter_write: %p, %d\n", in, inl);
  // /* Forward data to the write BIO */
  // long ret = BIO_write(bio->next_bio, in, inl);
  // dbgi("  -- %ld\n", ret);
  
  // /* Keep track of the packet, as we'll advertize them one by one after a pending check */
  // janus_dtls_bio_filter *filter = (janus_dtls_bio_filter *)bio->ptr;
  // if(filter != NULL) {
  //   xdtls_mutex_lock(&filter->mutex);
  //   filter->packets = g_list_append(filter->packets, GINT_TO_POINTER(ret));
  //   xdtls_mutex_unlock(&filter->mutex);
  //   dbgi("New list length: %d\n", g_list_length(filter->packets));
  // }
  // return ret;
}

long janus_dtls_bio_filter_ctrl(BIO *bio, int cmd, long num, void *ptr) {
  // see BIO_CTRL_RESET at https://github.com/openssl/openssl/blob/master/include/openssl/bio.h
  // 
  dbgi("janus_dtls_bio_filter_ctrl: cmd=%d, num=%ld", cmd, num);
  switch(cmd) {
    case BIO_CTRL_FLUSH:
      /* The OpenSSL library needs this */
      return 1;
    case BIO_CTRL_DGRAM_QUERY_MTU:
      /* Let's force the MTU that was configured */
      dbgi( "Advertizing MTU: %d\n", mtu);
      return mtu;
    case BIO_CTRL_WPENDING:
      return 0L;
    case BIO_CTRL_PENDING: {
      /* We only advertize one packet at a time, as they may be fragmented */
      janus_dtls_bio_filter *filter = (janus_dtls_bio_filter *)bio->ptr;
      if(filter == NULL)
        return 0;
      xdtls_mutex_lock(&filter->mutex);
      if(g_list_length(filter->packets) == 0) {
        xdtls_mutex_unlock(&filter->mutex);
        return 0;
      }
      /* Get the first packet that hasn't been read yet */
      GList *first = g_list_first(filter->packets);
      filter->packets = g_list_remove_link(filter->packets, first);
      int pending = GPOINTER_TO_INT(first->data);
      g_list_free(first);
      xdtls_mutex_unlock(&filter->mutex);
      /* We return its size so that only part of the buffer is read from the write BIO */
      return pending;
    }
    default: break;

  }
  return 0;
}





/* DTLS alert callback */
void janus_dtls_callback(const SSL *ssl, int where, int ret) {
  // see SSL_CB_LOOP at https://github.com/openssl/openssl/blob/c42a78cb57cd335f3e2b224d4d8c8d7c2ecfaa44/include/openssl/ssl.h
  dbgi("dtls_callbak: where=0x%X, ret = %d", where, ret);
  /* We only care about alerts */
  if (!(where & SSL_CB_ALERT)) {
    return;
  }
}

void janus_dtls_srtp_destroy(xdtls_srtp *dtls) {
}

xdtls_srtp *xdtls_srtp_create(void *ice_component, janus_dtls_role role) {
  // janus_ice_component *component = (janus_ice_component *)ice_component;
  // if(component == NULL) {
  //   JANUS_LOG(LOG_ERR, "No component, no DTLS...\n");
  //   return NULL;
  // }
  // janus_ice_stream *stream = component->stream;
  // if(!stream) {
  //   JANUS_LOG(LOG_ERR, "No stream, no DTLS...\n");
  //   return NULL;
  // }
  // janus_ice_handle *handle = stream->handle;
  // if(!handle || !handle->agent) {
  //   JANUS_LOG(LOG_ERR, "No handle/agent, no DTLS...\n");
  //   return NULL;
  // }
  xdtls_srtp *dtls = (xdtls_srtp *) g_malloc0(sizeof(xdtls_srtp));
  if(dtls == NULL) {
    dbge("Memory error!\n");
    return NULL;
  }
  /* Create SSL context, at last */
  dtls->srtp_valid = 0;
  dtls->ssl = SSL_new(xdtls_get_ssl_ctx());
  if(!dtls->ssl) {
    dbge("Error creating DTLS session! (%s)\n", ERR_reason_error_string(ERR_get_error()));
    janus_dtls_srtp_destroy(dtls);
    return NULL;
  }
  SSL_set_ex_data(dtls->ssl, 0, dtls);
  SSL_set_info_callback(dtls->ssl, janus_dtls_callback);
  dtls->read_bio = BIO_new(BIO_s_mem());
  if(!dtls->read_bio) {
    dbge("Error creating read BIO! (%s)\n", ERR_reason_error_string(ERR_get_error()));
    janus_dtls_srtp_destroy(dtls);
    return NULL;
  }
  BIO_set_mem_eof_return(dtls->read_bio, -1);
  dtls->write_bio = BIO_new(BIO_s_mem());
  if(!dtls->write_bio) {
    dbge("Error creating write BIO! (%s)\n", ERR_reason_error_string(ERR_get_error()));
    janus_dtls_srtp_destroy(dtls);
    return NULL;
  }
  BIO_set_mem_eof_return(dtls->write_bio, -1);
  /* The write BIO needs our custom filter, or fragmentation won't work */
  dtls->filter_bio = BIO_new(BIO_janus_dtls_filter());
  if(!dtls->filter_bio) {
    dbge("Error creating filter BIO! (%s)\n", ERR_reason_error_string(ERR_get_error()));
    janus_dtls_srtp_destroy(dtls);
    return NULL;
  }
  janus_dtls_bio_filter * filter = (janus_dtls_bio_filter *)dtls->filter_bio->ptr;
  filter ->dtls = dtls;
  /* Chain filter and write BIOs */
  BIO_push(dtls->filter_bio, dtls->write_bio);
  /* Set the filter as the BIO to use for outgoing data */
  SSL_set_bio(dtls->ssl, dtls->read_bio, dtls->filter_bio);
  dtls->dtls_role = role;
  if(dtls->dtls_role == JANUS_DTLS_ROLE_CLIENT) {
    dbgi("Setting connect state (DTLS client)\n");
    SSL_set_connect_state(dtls->ssl);
  } else {
    dbgi("Setting accept state (DTLS server)\n");
    SSL_set_accept_state(dtls->ssl);
  }
  /* https://code.google.com/p/chromium/issues/detail?id=406458 
   * Specify an ECDH group for ECDHE ciphers, otherwise they cannot be
   * negotiated when acting as the server. Use NIST's P-256 which is
   * commonly supported.
   */
  EC_KEY* ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if(ecdh == NULL) {
    dbge("Error creating ECDH group! (%s)\n", ERR_reason_error_string(ERR_get_error()));
    janus_dtls_srtp_destroy(dtls);
    return NULL;
  }
  SSL_set_options(dtls->ssl, SSL_OP_SINGLE_ECDH_USE);
  SSL_set_tmp_ecdh(dtls->ssl, ecdh);
  EC_KEY_free(ecdh);
  dtls->ready = 0;

  xdtls_mutex_init(&dtls->srtp_mutex);
  /* Done */
  dtls->dtls_connected = 0;
  // dtls->component = component;
  return dtls;
}

static
void print_buf_hex(const char * prefix, const unsigned char * data, int length){
  char msg[1024];
  char *rfp = (char *)&msg;
  unsigned int i = 0;
  for(i = 0; i < length; i++) {
    rfp += g_snprintf(rfp, 4, "%.2X:", data[i]);
    // rfp += 3;
  }
  *(rfp-1) = 0;
  dbgi("%s%s", prefix, msg);
}

static
void setup_srtp(xdtls_srtp * dtls){
  X509 *rcert = SSL_get_peer_certificate(dtls->ssl);
  if(!rcert) {
    dbge( "No remote certificate?? (%s)\n",  ERR_reason_error_string(ERR_get_error()));
    return;
  }
      unsigned int rsize;
      unsigned char rfingerprint[EVP_MAX_MD_SIZE];
      
      
      X509_digest(rcert, EVP_sha256(), (unsigned char *)rfingerprint, &rsize);
      rcert = NULL;

      print_buf_hex("Remote fingerprint(sha-256): ", rfingerprint, rsize);

      /* Complete with SRTP setup */
      unsigned char material[SRTP_MASTER_LENGTH*2];
      unsigned char *local_key, *local_salt, *remote_key, *remote_salt;
      memset(material, 0, SRTP_MASTER_LENGTH*2 );
      /* Export keying material for SRTP */
      if (!SSL_export_keying_material(dtls->ssl, material, SRTP_MASTER_LENGTH*2, "EXTRACTOR-dtls_srtp", 19, NULL, 0, 0)) {
        /* Oops... */
        dbge(" Oops, couldn't extract SRTP keying material ?? (%s)", ERR_reason_error_string(ERR_get_error()));
        return;
      }

      /* Key derivation (http://tools.ietf.org/html/rfc5764#section-4.2) */

      if(dtls->dtls_role == JANUS_DTLS_ROLE_CLIENT) {
        local_key = material;
        remote_key = local_key + SRTP_MASTER_KEY_LENGTH;
        local_salt = remote_key + SRTP_MASTER_KEY_LENGTH;
        remote_salt = local_salt + SRTP_MASTER_SALT_LENGTH;
      } else {
        remote_key = material;
        local_key = remote_key + SRTP_MASTER_KEY_LENGTH;
        remote_salt = local_key + SRTP_MASTER_KEY_LENGTH;
        local_salt = remote_salt + SRTP_MASTER_SALT_LENGTH;
      }

      print_buf_hex("local_key: ", local_key, SRTP_MASTER_KEY_LENGTH);
      print_buf_hex("local_salt: ", local_salt, SRTP_MASTER_SALT_LENGTH);
      print_buf_hex("remote_key: ", remote_key, SRTP_MASTER_KEY_LENGTH);
      print_buf_hex("remote_salt: ", remote_salt, SRTP_MASTER_SALT_LENGTH);

      /* Build master keys and set SRTP policies */
            /* Remote (inbound) */
          srtp_crypto_policy_set_rtp_default(&(dtls->remote_policy.rtp));
          srtp_crypto_policy_set_rtcp_default(&(dtls->remote_policy.rtcp));
          dtls->remote_policy.ssrc.type = ssrc_any_inbound;
          unsigned char remote_policy_key[SRTP_MASTER_LENGTH];
          dtls->remote_policy.key = (unsigned char *)&remote_policy_key;
          memcpy(dtls->remote_policy.key, remote_key, SRTP_MASTER_KEY_LENGTH);
          memcpy(dtls->remote_policy.key + SRTP_MASTER_KEY_LENGTH, remote_salt, SRTP_MASTER_SALT_LENGTH);
#if HAS_DTLS_WINDOW_SIZE
          dtls->remote_policy.window_size = 128;
          dtls->remote_policy.allow_repeat_tx = 0;
#endif
          dtls->remote_policy.next = NULL;
            /* Local (outbound) */
          srtp_crypto_policy_set_rtp_default(&(dtls->local_policy.rtp));
          srtp_crypto_policy_set_rtcp_default(&(dtls->local_policy.rtcp));
          dtls->local_policy.ssrc.type = ssrc_any_outbound;
          unsigned char local_policy_key[SRTP_MASTER_LENGTH];
          dtls->local_policy.key = (unsigned char *)&local_policy_key;
          memcpy(dtls->local_policy.key, local_key, SRTP_MASTER_KEY_LENGTH);
          memcpy(dtls->local_policy.key + SRTP_MASTER_KEY_LENGTH, local_salt, SRTP_MASTER_SALT_LENGTH);
#if HAS_DTLS_WINDOW_SIZE
          dtls->local_policy.window_size = 128;
          dtls->local_policy.allow_repeat_tx = 0;
#endif
          dtls->local_policy.next = NULL;
          /* Create SRTP sessions */
          srtp_err_status_t res = srtp_create(&(dtls->srtp_in), &(dtls->remote_policy));
          dbge("creating inbound srtp %s", res == srtp_err_status_ok ? "ok" : "fail");
          res = srtp_create(&(dtls->srtp_out), &(dtls->local_policy));
          dbge("creating outbound srtp %s", res == srtp_err_status_ok ? "ok" : "fail");
          dtls->srtp_valid = 1;
        

          char sbuf[1024];
          int slen = 0;
          sbuf[0] = 0x80;
          sbuf[1] = 97; // payload type
          sbuf[1] |= 0x80; // M 
          sbuf[2] = 0; sbuf[3] = 0; // seq
          sbuf[4] = sbuf[5] = sbuf[6] = sbuf[7] = 0; // timestamp
          sbuf[8] = sbuf[9] = sbuf[10] = sbuf[11] = 0; // ssrc
          slen = sprintf(sbuf + 12, "Hi from %s", dtls->dtls_role == JANUS_DTLS_ROLE_CLIENT ? "client" : "server");
          slen += 12;
          dbgi("rtp len %d", slen);
          res = srtp_protect(dtls->srtp_out, sbuf, &slen);
          if(res != srtp_err_status_ok){
            dbge("srtp_protect fail, res=%d", res);
            return ;
          }
          dbgi("rtp protect len %d", slen);

    GError *error = NULL;
    gssize sent_bytes = g_socket_send_to (dtls->sock,
                  dtls->peer_addr,
                  sbuf,
                  slen,
                  NULL,
                  &error);

}

static 
gboolean dtls_gio_in (GIOChannel *source, GIOCondition cond, gpointer data)
{
  // g_print("udp_gio_in_echo\n");

  xdtls_srtp * dtls = (xdtls_srtp *) data;
  GSocket *sock = dtls->sock;
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

    g_print ("recv from %s, %ld bytes\n", addrstr, len);

    if(!dtls->peer_addr){
      dtls->peer_addr = (GSocketAddress *)g_object_ref(addr);
    }

    unsigned char first_byte = buf[0];
    if(first_byte > 127 && first_byte < 192){
      if(dtls->srtp_valid){
        int buflen = len;
        srtp_err_status_t res = srtp_unprotect(dtls->srtp_in, buf, &buflen);
        if(res != srtp_err_status_ok){
          dbge("srtp_unprotect fail, res=%d", res);
          return TRUE;
        }
        buf[buflen] = 0;
        dbgi("rtp unprotect len %d, msg=%s", buflen, buf+12);
      }else{
        dbgi("got rtp packet before srtp_valid");
      }
      return TRUE;
    }

    int written = BIO_write(dtls->read_bio, buf, len);
    dbgi("write read_bio %d/%ld", written, len);

    char data[1500];  /* FIXME */
    memset(&data, 0, 1500);
    int read = SSL_read(dtls->ssl, &data, 1500);
    dbgi("after SSL_read");
    int init_done = SSL_is_init_finished(dtls->ssl);
    dbgi("ssl read=%d, init_done=%d, state=%s (SSL_ST_OK=%d)", read, init_done, SSL_state_string(dtls->ssl), SSL_get_state(dtls->ssl)==SSL_ST_OK);
    if(init_done && !dtls->handshake_done){
      dtls->handshake_done = 1;
      setup_srtp(dtls);

    }

  }else{
    g_print ("recv error: %ld\n", len);
  }

  if (addr != NULL){
    g_object_unref (addr);
  }
      
  // g_main_loop_quit(gloop);

  return TRUE;
}






static
gboolean idleCpt (gpointer user_data){

   int *a = (int *)user_data;
   g_print("%d\n", *a);
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
  int ret = -1;

  int is_client = 0;
  if(argc > 1){
    is_client = atoi(argv[1]);
  }
  dbgi("is_client = %d", is_client);

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


  janus_dtls_role role = JANUS_DTLS_ROLE_SERVER;
  if(is_client){
    role = JANUS_DTLS_ROLE_CLIENT;
    udp_port++;
  }
  xdtls_srtp * dtls = xdtls_srtp_create(NULL, role) ;
  if(!dtls){
    return 2;
  }



  GSocketAddress * gsockAddr = G_SOCKET_ADDRESS(g_inet_socket_address_new(g_inet_address_new_any(G_SOCKET_FAMILY_IPV4), udp_port));
  s_udp = g_socket_new(G_SOCKET_FAMILY_IPV4,
                    G_SOCKET_TYPE_DATAGRAM,
                    G_SOCKET_PROTOCOL_UDP,
                    &err);
  g_assert(err == NULL);

  if (s_udp == NULL) {
    g_print("ERREUR");
    exit(1);
  }

  g_socket_set_blocking (s_udp, FALSE);

  if(!is_client){
    if (g_socket_bind (s_udp, gsockAddr, TRUE, NULL) == FALSE){

     g_print("Error bind server\n");
     exit(1);

    }
  }else{
    if (g_socket_bind (s_udp, gsockAddr, FALSE, NULL) == FALSE){

     g_print("Error bind client\n");
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
  source = g_io_add_watch(channel, G_IO_IN, (GIOFunc) dtls_gio_in, dtls); 
  

  g_io_channel_unref(channel);
  g_message("<%lld> local port %d\n", (long long)pthread_self(), udp_port);

  dtls->sock = s_udp;

  if(is_client){
    dtls->peer_addr = G_SOCKET_ADDRESS(g_inet_socket_address_new_from_string("127.0.0.1", server_port));
    // gssize sent_bytes = g_socket_send_to (s_udp,
    //               dtls->peer_addr,
    //               "123",
    //               4,
    //               NULL,
    //               &err);

  dbgi("start dtls =>");
  SSL_do_handshake(dtls->ssl);
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

