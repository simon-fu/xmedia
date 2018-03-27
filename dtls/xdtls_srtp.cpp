



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

#include <pthread.h>
#define dbgv(...) do{ printf("|%lx|", (long)pthread_self()); printf("<dtls>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{ printf("|%lx|", (long)pthread_self()); printf("<dtls>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{ printf("|%lx|", (long)pthread_self()); printf("<dtls>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)

#define ZERO_ALLOC_(o, type, sz) do{o=(type)malloc(sz); memset(o, 0, sz);}while(0)



//
// DTLS and SRTP define
//
#define DTLS_CIPHERS  "ALL:NULL:eNULL:aNULL"
/* SRTP stuff (http://tools.ietf.org/html/rfc3711) */
#define SRTP_MASTER_KEY_LENGTH  16
#define SRTP_MASTER_SALT_LENGTH 14
#define SRTP_MASTER_LENGTH (SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_SALT_LENGTH)


//
// lock define
//
typedef pthread_mutex_t xdtls_mutex;
static int lock_debug = 0;
#define xdtls_mutex_init(a) pthread_mutex_init(a,NULL)
#define xdtls_mutex_destroy(a) pthread_mutex_destroy(a)
/*! \brief Janus mutex lock without debug */
#define xdtls_mutex_lock_nodebug(a) pthread_mutex_lock(a);
/*! \brief Janus mutex lock with debug (prints the line that locked a mutex) */
#define xdtls_mutex_lock_debug(a) { dbgi("[%s:%s:%d:] ", __FILE__, __FUNCTION__, __LINE__); dbgi("LOCK %p\n", a); pthread_mutex_lock(a); };
/*! \brief Janus mutex lock wrapper (selective locking debug) */
#define xdtls_mutex_lock(a) { if(!lock_debug) { xdtls_mutex_lock_nodebug(a); } else { xdtls_mutex_lock_debug(a); } };
/*! \brief Janus mutex unlock without debug */
#define xdtls_mutex_unlock_nodebug(a) pthread_mutex_unlock(a);
/*! \brief Janus mutex unlock with debug (prints the line that unlocked a mutex) */
#define xdtls_mutex_unlock_debug(a) { dbgi("[%s:%s:%d:] ", __FILE__, __FUNCTION__, __LINE__); dbgi("UNLOCK %p\n", a); pthread_mutex_unlock(a); };
#define xdtls_mutex_unlock(a) { if(!lock_debug) { xdtls_mutex_unlock_nodebug(a); } else { xdtls_mutex_unlock_debug(a); } };


/*! \brief DTLS state */
typedef enum xdtls_state {
    XDTLS_STATE_FAILED = -1,
    XDTLS_STATE_CREATED,
    XDTLS_STATE_TRYING,
    XDTLS_STATE_CONNECTED,
} xdtls_state;


typedef struct xdtls_srtp_st {
    SSL_CTX * ssl_ctx;
    
    unsigned char fingerprint[EVP_MAX_MD_SIZE]; // EVP_MAX_MD_SIZE = 64
    int  fingerprint_len;
    char fingerprint_str[EVP_MAX_MD_SIZE*3+3];
    int fingerprint_str_len;
    
    /*! \brief Opaque pointer to the component this DTLS-SRTP context belongs to */
    void *component;
    /*! \brief DTLS role of the gateway for this stream: 1=client, 0=server */
    xdtls_role dtls_role;
    /*! \brief DTLS state of this component: -1=failed, 0=nothing, 1=trying, 2=connected */
    xdtls_state dtls_state;
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
//    xdtls_mutex srtp_mutex;
    /*! \brief Whether this DTLS stack is now ready to be used for messages as well (e.g., SCTP encapsulation) */
    int ready;
    
    xdtls_srtp_send_func_t send_func;
    void * cb_context;
    int handshake_done;
    
//    GSocket *sock;
//    GSocketAddress * peer_addr;
    
    
} xdtls_srtp;








//
// global var
//
static xdtls_mutex * g_dtls_locks = NULL;;
static const char * g_pem_file = NULL;
static const char * g_key_file = NULL;
static int mtu = 1472;

static
int buf_to_hex_str_(const char * prefix, const unsigned char * data, int length, char* str_buf){
    char *p = (char *)str_buf;
    unsigned int i = 0;
    if(prefix){
        while(*prefix){
            *p = *prefix;
            prefix++;
            p++;
        }
    }
    for(i = 0; i < length; i++) {
        p += snprintf(p, 4,"%.2X:", data[i]);
        // p += 3;
    }
    *(p-1) = 0;
    return p-str_buf-1;
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



int xdtls_srtp_init(const char *pem_file, const char *key_file) {
    /* FIXME First of all make OpenSSL thread safe (see note above on issue #316) */
    ZERO_ALLOC_(g_dtls_locks, xdtls_mutex *, (sizeof(*g_dtls_locks) * CRYPTO_num_locks()));
    // g_dtls_locks = (xdtls_mutex *) malloc(sizeof(*g_dtls_locks) * CRYPTO_num_locks());
    int l=0;
    for(l = 0; l < CRYPTO_num_locks(); l++) {
        xdtls_mutex_init(&g_dtls_locks[l]);
    }
    
    g_pem_file = strdup(pem_file);
    g_key_file = strdup(key_file);
    
    
    CRYPTO_THREADID_set_callback(xdtls_cb_openssl_threadid);
    CRYPTO_set_locking_callback(xdtls_cb_openssl_lock);
    
    OpenSSL_add_ssl_algorithms();
    SSL_load_error_strings();
    
    /* Initialize libsrtp */
    if(srtp_init() != srtp_err_status_ok) {
        dbge("Ops, error setting up libsrtp?\n");
        return -5;
    }
    
    return 0;
}





/* Filter implementation */
static int janus_dtls_bio_filter_write(BIO *h, const char *buf,int num);
static long janus_dtls_bio_filter_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int janus_dtls_bio_filter_new(BIO *h);
static int janus_dtls_bio_filter_free(BIO *data);

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
static BIO_METHOD *BIO_janus_dtls_filter(void) {
    return(&janus_dtls_bio_filter_methods);
}


/* Helper struct to keep the filter state */
typedef struct janus_dtls_bio_filter {
    GList *packets;
    xdtls_mutex mutex;
    xdtls_srtp * dtls;
} janus_dtls_bio_filter;


static int janus_dtls_bio_filter_new(BIO *bio) {
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

static int janus_dtls_bio_filter_free(BIO *bio) {
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

static int janus_dtls_bio_filter_write(BIO *bio, const char *in, int inl) {
    
    janus_dtls_bio_filter *filter = (janus_dtls_bio_filter *)bio->ptr;
    
    if(filter->dtls->send_func){
        dbgi("janus_dtls_bio_filter_write: send %p, %d\n", in, inl);
        filter->dtls->send_func(filter->dtls, filter->dtls->cb_context, (const unsigned char *)in, inl);
    }else{
        dbgi("janus_dtls_bio_filter_write: skip %p, %d\n", in, inl);
    }
    return inl;
}

static long janus_dtls_bio_filter_ctrl(BIO *bio, int cmd, long num, void *ptr) {
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
static void janus_dtls_callback(const SSL *ssl, int where, int ret) {
    // see SSL_CB_LOOP at https://github.com/openssl/openssl/blob/c42a78cb57cd335f3e2b224d4d8c8d7c2ecfaa44/include/openssl/ssl.h
    dbgi("dtls_callbak: where=0x%X, ret = %d", where, ret);
    /* We only care about alerts */
    if (!(where & SSL_CB_ALERT)) {
        return;
    }
}

void xdtls_srtp_destroy(xdtls_srtp_t dtls) {
}

/* DTLS certificate verification callback */
static
int xdtls_verify_callback(int preverify_ok, X509_STORE_CTX *ctx) {
    /* We just use the verify_callback to request a certificate from the client */
    dbgi("xdtls_verify_callback: preverify_ok=%d, x509ctx=%p", preverify_ok, ctx);
    return 1;
}

xdtls_srtp_t xdtls_srtp_create(const char *pem_file, const char *key_file, xdtls_role role, xdtls_srtp_send_func_t send_func, void * cb_context) {
    xdtls_srtp *dtls = NULL;
    BIO *certbio = NULL;
    X509 *cert = NULL;
    EC_KEY* ecdh = NULL;
    int ret = -1;
    
    do{
        if(!pem_file){
            pem_file = g_pem_file;
        }
        if(!key_file){
            key_file = g_key_file;
        }
        
        if(!pem_file || !key_file){
            dbge("must specify pem and key file");
            ret = -1;
            break;
        }
        
        ZERO_ALLOC_(dtls, xdtls_srtp_t, sizeof(xdtls_srtp));
        if(!dtls) {
            dbge("Memory error!\n");
            ret = -1;
            break;
        }
        dtls->send_func = send_func;
        dtls->cb_context = cb_context;
        
        
        /* create the DTLS context */
        dtls->ssl_ctx = SSL_CTX_new(DTLSv1_method());
        // dtls->ssl_ctx = SSL_CTX_new(DTLSv1_server_method());
        if(!dtls->ssl_ctx) {
            dbge("Ops, error creating DTLS context?\n");
            ret =  -1;
            break;
        }
        
        SSL_CTX_set_verify(dtls->ssl_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, xdtls_verify_callback);
        SSL_CTX_set_tlsext_use_srtp(dtls->ssl_ctx, "SRTP_AES128_CM_SHA1_80"); /* FIXME Should we support something else as well? */
        
        // load pem and key file, and check them
        if(!SSL_CTX_use_certificate_file(dtls->ssl_ctx, pem_file, SSL_FILETYPE_PEM)) {
            dbge("Certificate error (%s)\n", ERR_reason_error_string(ERR_get_error()));
            ret = -2;
            break;
        }
        if(!SSL_CTX_use_PrivateKey_file(dtls->ssl_ctx, key_file, SSL_FILETYPE_PEM)) {
            dbge("Certificate key error (%s)\n", ERR_reason_error_string(ERR_get_error()));
            ret = -3;
            break;
        }
        if(!SSL_CTX_check_private_key(dtls->ssl_ctx)) {
            dbge("Certificate check error (%s)\n", ERR_reason_error_string(ERR_get_error()));
            ret = -4;
            break;
        }
        
        // enable read ahead for good performance
        SSL_CTX_set_read_ahead(dtls->ssl_ctx,1);
        
        
        // read pem file and get local fingerprint
        certbio = BIO_new(BIO_s_file());
        if(certbio == NULL) {
            dbge( "Certificate BIO error...\n");
            ret = -5;
            break;
        }
        if(BIO_read_filename(certbio, pem_file) == 0) {
            dbge("Error reading certificate (%s)\n", ERR_reason_error_string(ERR_get_error()));
            ret = -6;
            break;
        }
        cert = PEM_read_bio_X509(certbio, NULL, 0, NULL);
        if(cert == NULL) {
            dbge("Error reading certificate (%s)\n", ERR_reason_error_string(ERR_get_error()));
            ret = -7;
            break;
        }
        
        unsigned int size = EVP_MAX_MD_SIZE;
        if(X509_digest(cert, EVP_sha256(), dtls->fingerprint, &size) == 0) {
            dbge( "Error converting X509 structure (%s)\n", ERR_reason_error_string(ERR_get_error()));
            ret = -7;
            break;
        }
        dtls->fingerprint_len = size;
        
        dtls->fingerprint_str_len = buf_to_hex_str_(NULL, dtls->fingerprint, dtls->fingerprint_len, dtls->fingerprint_str);
        dbgi("local fingerprint (%d bytes): %s", dtls->fingerprint_str_len, dtls->fingerprint_str);

        
        SSL_CTX_set_cipher_list(dtls->ssl_ctx, DTLS_CIPHERS);
        
        
        /* Create SSL */
        dtls->ssl = SSL_new(dtls->ssl_ctx);
        if(!dtls->ssl) {
            dbge("Error creating DTLS session! (%s)\n", ERR_reason_error_string(ERR_get_error()));
            ret = -10;
            break;
        }
        SSL_set_ex_data(dtls->ssl, 0, dtls);
        SSL_set_info_callback(dtls->ssl, janus_dtls_callback);
        
        dtls->read_bio = BIO_new(BIO_s_mem());
        if(!dtls->read_bio) {
            dbge("Error creating read BIO! (%s)\n", ERR_reason_error_string(ERR_get_error()));
            ret = -20;
            break;
        }
        BIO_set_mem_eof_return(dtls->read_bio, -1);
        
        dtls->write_bio = BIO_new(BIO_s_mem());
        if(!dtls->write_bio) {
            dbge("Error creating write BIO! (%s)\n", ERR_reason_error_string(ERR_get_error()));
            ret = -30;
            break;
        }
        BIO_set_mem_eof_return(dtls->write_bio, -1);
        
        /* The write BIO needs our custom filter, or fragmentation won't work */
        dtls->filter_bio = BIO_new(BIO_janus_dtls_filter());
        if(!dtls->filter_bio) {
            dbge("Error creating filter BIO! (%s)\n", ERR_reason_error_string(ERR_get_error()));
            ret = -40;
            break;
        }
        janus_dtls_bio_filter * filter = (janus_dtls_bio_filter *)dtls->filter_bio->ptr;
        filter ->dtls = dtls;
        
        /* Chain filter and write BIOs */
        BIO_push(dtls->filter_bio, dtls->write_bio);
        /* Set the filter as the BIO to use for outgoing data */
        SSL_set_bio(dtls->ssl, dtls->read_bio, dtls->filter_bio);
        dtls->dtls_role = role;
        if(dtls->dtls_role == XDTLS_ROLE_CLIENT) {
            dbgi("Setting connect state (DTLS client)");
            SSL_set_connect_state(dtls->ssl);
        } else {
            dbgi("Setting accept state (DTLS server)");
            SSL_set_accept_state(dtls->ssl);
        }
        
        /* https://code.google.com/p/chromium/issues/detail?id=406458
         * Specify an ECDH group for ECDHE ciphers, otherwise they cannot be
         * negotiated when acting as the server. Use NIST's P-256 which is
         * commonly supported.
         */
        ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
        if(ecdh == NULL) {
            dbge("Error creating ECDH group! (%s)\n", ERR_reason_error_string(ERR_get_error()));
            ret = -50;
            break;
        }
        SSL_set_options(dtls->ssl, SSL_OP_SINGLE_ECDH_USE);
        SSL_set_tmp_ecdh(dtls->ssl, ecdh);
        dtls->ready = 0;
        
//        xdtls_mutex_init(&dtls->srtp_mutex);
        ret = 0;

    }while(0);
    
    if(ecdh){
        EC_KEY_free(ecdh);
    }
    if(cert){
        X509_free(cert);
    }
    if(certbio){
        BIO_free_all(certbio);
    }
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
    
    if(dtls->dtls_role == XDTLS_ROLE_CLIENT) {
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
    dbgi("creating inbound srtp %s", res == srtp_err_status_ok ? "ok" : "fail");
    res = srtp_create(&(dtls->srtp_out), &(dtls->local_policy));
    dbgi("creating outbound srtp %s", res == srtp_err_status_ok ? "ok" : "fail");
    dtls->srtp_valid = 1;
    
    
//    char sbuf[1024];
//    int slen = 0;
//    sbuf[0] = 0x80;
//    sbuf[1] = 97; // payload type
//    sbuf[1] |= 0x80; // M 
//    sbuf[2] = 0; sbuf[3] = 0; // seq
//    sbuf[4] = sbuf[5] = sbuf[6] = sbuf[7] = 0; // timestamp
//    sbuf[8] = sbuf[9] = sbuf[10] = sbuf[11] = 0; // ssrc
//    slen = sprintf(sbuf + 12, "Hi from %s", dtls->dtls_role == JANUS_DTLS_ROLE_CLIENT ? "client" : "server");
//    slen += 12;
//    dbgi("rtp len %d", slen);
//    res = srtp_protect(dtls->srtp_out, sbuf, &slen);
//    if(res != srtp_err_status_ok){
//        dbge("srtp_protect fail, res=%d", res);
//        return ;
//    }
//    dbgi("rtp protect len %d", slen);
//    
//    GError *error = NULL;
//    gssize sent_bytes = g_socket_send_to (dtls->sock,
//                                          dtls->peer_addr,
//                                          sbuf,
//                                          slen,
//                                          NULL,
//                                          &error);
    
}


int xdtls_srtp_start_handshake(xdtls_srtp_t dtls){
    SSL_do_handshake(dtls->ssl);
    return 0;
}


int xdtls_srtp_input_data(xdtls_srtp_t dtls, const unsigned char * buf, int len){
    int written = BIO_write(dtls->read_bio, buf, len);
    dbgi("write read_bio %d/%d", written, len);
    
    char data[1500];  /* FIXME */
    memset(&data, 0, 1500);
    int read = SSL_read(dtls->ssl, &data, 1500);
    int init_done = SSL_is_init_finished(dtls->ssl);
    dbgi("ssl read=%d, init_done=%d, state=%s (SSL_ST_OK=%d)", read, init_done, SSL_state_string(dtls->ssl), SSL_get_state(dtls->ssl)==SSL_ST_OK);
    if(init_done && !dtls->handshake_done){
        dtls->handshake_done = 1;
        setup_srtp(dtls);
    }
    return 0;
}


int xdtls_srtp_is_ready(xdtls_srtp_t dtls){
    return dtls->srtp_valid;
}

int xdtls_srtp_protect(xdtls_srtp_t dtls, unsigned char * buf, int len){
    if(!dtls->srtp_valid){
        return -1;
    }
    
    srtp_err_status_t res = srtp_protect(dtls->srtp_out, buf, &len);
    if(res != srtp_err_status_ok){
        dbge("srtp_protect fail, res=%d", res);
        return -2;
    }
//    dbgi("rtp protect len %d", len);
    return len;
}

int xdtls_srtp_unprotect(xdtls_srtp_t dtls, unsigned char * buf, int len){
    if(!dtls->srtp_valid){
        return -1;
    }
    
    int buflen = len;
    srtp_err_status_t res = srtp_unprotect(dtls->srtp_in, buf, &buflen);
    if(res != srtp_err_status_ok){
        dbge("srtp_unprotect fail, res=%d", res);
        return -2;
    }
//    buf[buflen] = 0;
//    dbgi("rtp unprotect len %d, msg=%s", buflen, buf+12);
    return buflen;
}

