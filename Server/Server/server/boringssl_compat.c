/*
 * boringssl_compat.c
 *
 * DataPipes is compiled against BoringSSL, where these APIs are exported as
 * real functions.  When linking TeaSpeakServer against OpenSSL (prebuild or
 * system), the linker cannot resolve them because OpenSSL provides them only
 * as macros (no exported symbols).
 *
 * This file provides thin wrapper implementations so the linker can satisfy
 * those references without requiring BoringSSL at link time.
 *
 * Deliberately includes NO OpenSSL/BoringSSL headers to avoid macro-name
 * conflicts; instead uses forward declarations and hard-coded constants that
 * match the corresponding OpenSSL macro expansions.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Opaque type stubs -------------------------------------------- */
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st     SSL;
typedef struct bio_st     BIO;

/* ---------- OpenSSL functions we delegate to ------------------------------ */
/* (These are real exported symbols in every OpenSSL 1.x / 3.x build.) */
extern long SSL_CTX_callback_ctrl(SSL_CTX *, int, void (*)(void));
extern long SSL_CTX_ctrl(SSL_CTX *, int, long, void *);
extern long SSL_ctrl(SSL *, int, long, void *);
extern void BIO_set_flags(BIO *b, int flags);
extern void BIO_clear_flags(BIO *b, int flags);

/* ---------- Constants (tls1.h / bio.h) ------------------------------------ */
#define _SSL_CTRL_SET_TLSEXT_SERVERNAME_CB   53
#define _SSL_CTRL_SET_TLSEXT_SERVERNAME_ARG  54
#define _SSL_CTRL_SET_TLSEXT_HOSTNAME        55
#define _TLSEXT_NAMETYPE_host_name           0
#define _BIO_FLAGS_READ         0x01
#define _BIO_FLAGS_RWS          (0x01 | 0x02 | 0x04)
#define _BIO_FLAGS_SHOULD_RETRY 0x08

/* ---------- Missing BoringSSL symbol implementations --------------------- */

int SSL_CTX_set_tlsext_servername_callback(SSL_CTX *ctx,
                                           int (*cb)(SSL *, int *, void *))
{
    return (int)SSL_CTX_callback_ctrl(ctx,
                                      _SSL_CTRL_SET_TLSEXT_SERVERNAME_CB,
                                      (void (*)(void))cb);
}

int SSL_CTX_set_tlsext_servername_arg(SSL_CTX *ctx, void *arg)
{
    return (int)SSL_CTX_ctrl(ctx, _SSL_CTRL_SET_TLSEXT_SERVERNAME_ARG, 0, arg);
}

int SSL_set_tlsext_host_name(SSL *ssl, const char *name)
{
    return (int)SSL_ctrl(ssl,
                         _SSL_CTRL_SET_TLSEXT_HOSTNAME,
                         _TLSEXT_NAMETYPE_host_name,
                         (void *)(unsigned long)name);
}

void BIO_clear_retry_flags(BIO *b)
{
    BIO_clear_flags(b, (_BIO_FLAGS_RWS | _BIO_FLAGS_SHOULD_RETRY));
}

void BIO_set_retry_read(BIO *b)
{
    BIO_set_flags(b, (_BIO_FLAGS_READ | _BIO_FLAGS_SHOULD_RETRY));
}

#ifdef __cplusplus
} /* extern "C" */
#endif
