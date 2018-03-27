

#ifndef xdtls_srtp_hpp
#define xdtls_srtp_hpp

typedef struct xdtls_srtp_st * xdtls_srtp_t;

/*! \brief DTLS roles */
typedef enum xdtls_role {
    XDTLS_ROLE_ACTPASS = -1,
    XDTLS_ROLE_SERVER,
    XDTLS_ROLE_CLIENT,
} xdtls_role;


typedef int (*xdtls_srtp_send_func_t)(xdtls_srtp_t dtls, void * cb_context, const unsigned char * data, int length);

int xdtls_srtp_init(const char *pem_file, const char *key_file);
xdtls_srtp_t xdtls_srtp_create(const char *pem_file, const char *key_file, xdtls_role role, xdtls_srtp_send_func_t send_func, void * cb_context);
void xdtls_srtp_destroy(xdtls_srtp_t dtls);
int xdtls_srtp_start_handshake(xdtls_srtp_t dtls);
int xdtls_srtp_input_data(xdtls_srtp_t dtls, const unsigned char * buf, int len);
int xdtls_srtp_is_ready(xdtls_srtp_t dtls);
int xdtls_srtp_protect(xdtls_srtp_t dtls, unsigned char * buf, int len);
int xdtls_srtp_unprotect(xdtls_srtp_t dtls, unsigned char * buf, int len);

#endif /* xdtls_srtp_hpp */
