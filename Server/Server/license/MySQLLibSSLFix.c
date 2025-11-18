#include <openssl/aes.h>
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

const EVP_CIPHER *EVP_aes_128_cfb1(void){ return 0; }
const EVP_CIPHER *EVP_aes_192_cfb1(void){ return 0; }
const EVP_CIPHER *EVP_aes_256_cfb1(void){ return 0; }

const EVP_CIPHER *EVP_aes_128_cfb8(void){ return 0; }
const EVP_CIPHER *EVP_aes_192_cfb8(void){ return 0; }
const EVP_CIPHER *EVP_aes_256_cfb8(void){ return 0; }

const EVP_CIPHER *EVP_aes_128_cfb128(void){ return 0; }
const EVP_CIPHER *EVP_aes_192_cfb128(void){ return 0; }
const EVP_CIPHER *EVP_aes_256_cfb128(void){ return 0; }

int EVP_EncryptFinal(EVP_CIPHER_CTX *ctx, uint8_t *out, int *out_len) {
	return EVP_EncryptFinal_ex(ctx, out, out_len);
}

int EVP_DecryptFinal(EVP_CIPHER_CTX *ctx, unsigned char *out, int *out_len) {
	return EVP_DecryptFinal_ex(ctx, out, out_len);
}

int SSL_CTX_set_ciphersuites(SSL_CTX *ctx, const char *str) {
	return 0;
}

#define DTLSv1_get_timeout DTLSv1_get_timeout
#define DTLSv1_handle_timeout DTLSv1_handle_timeout
#define SSL_CTX_add0_chain_cert SSL_CTX_add0_chain_cert
#define SSL_CTX_add1_chain_cert SSL_CTX_add1_chain_cert
#define SSL_CTX_add_extra_chain_cert SSL_CTX_add_extra_chain_cert
#define SSL_CTX_clear_extra_chain_certs SSL_CTX_clear_extra_chain_certs
#define SSL_CTX_clear_chain_certs SSL_CTX_clear_chain_certs
#define SSL_CTX_clear_mode SSL_CTX_clear_mode
#define SSL_CTX_clear_options SSL_CTX_clear_options
#define SSL_CTX_get0_chain_certs SSL_CTX_get0_chain_certs
#define SSL_CTX_get_extra_chain_certs SSL_CTX_get_extra_chain_certs
#define SSL_CTX_get_max_cert_list SSL_CTX_get_max_cert_list
#define SSL_CTX_get_mode SSL_CTX_get_mode
#define SSL_CTX_get_options SSL_CTX_get_options
#define SSL_CTX_get_read_ahead SSL_CTX_get_read_ahead
#define SSL_CTX_get_session_cache_mode SSL_CTX_get_session_cache_mode
#define SSL_CTX_get_tlsext_ticket_keys SSL_CTX_get_tlsext_ticket_keys
#define SSL_CTX_need_tmp_RSA SSL_CTX_need_tmp_RSA
#define SSL_CTX_sess_get_cache_size SSL_CTX_sess_get_cache_size
#define SSL_CTX_sess_number SSL_CTX_sess_number
#define SSL_CTX_sess_set_cache_size SSL_CTX_sess_set_cache_size
#define SSL_CTX_set0_chain SSL_CTX_set0_chain
#define SSL_CTX_set1_chain SSL_CTX_set1_chain
#define SSL_CTX_set1_curves SSL_CTX_set1_curves
#define SSL_CTX_set_max_cert_list SSL_CTX_set_max_cert_list
#define SSL_CTX_set_max_send_fragment SSL_CTX_set_max_send_fragment
#define SSL_CTX_set_mode SSL_CTX_set_mode
#define SSL_CTX_set_msg_callback_arg SSL_CTX_set_msg_callback_arg
#define SSL_CTX_set_options SSL_CTX_set_options
#define SSL_CTX_set_read_ahead SSL_CTX_set_read_ahead
#define SSL_CTX_set_session_cache_mode SSL_CTX_set_session_cache_mode
#define SSL_CTX_set_tlsext_servername_arg SSL_CTX_set_tlsext_servername_arg
#define SSL_CTX_set_tlsext_servername_callback \
    SSL_CTX_set_tlsext_servername_callback
#define SSL_CTX_set_tlsext_ticket_key_cb SSL_CTX_set_tlsext_ticket_key_cb
#define SSL_CTX_set_tlsext_ticket_keys SSL_CTX_set_tlsext_ticket_keys
#define SSL_CTX_set_tmp_dh SSL_CTX_set_tmp_dh
#define SSL_CTX_set_tmp_ecdh SSL_CTX_set_tmp_ecdh
#define SSL_CTX_set_tmp_rsa SSL_CTX_set_tmp_rsa
#define SSL_add0_chain_cert SSL_add0_chain_cert
#define SSL_add1_chain_cert SSL_add1_chain_cert
#define SSL_clear_chain_certs SSL_clear_chain_certs
#define SSL_clear_mode SSL_clear_mode
#define SSL_clear_options SSL_clear_options
#define SSL_get0_certificate_types SSL_get0_certificate_types
#define SSL_get0_chain_certs SSL_get0_chain_certs
#define SSL_get_max_cert_list SSL_get_max_cert_list
#define SSL_get_mode SSL_get_mode
#define SSL_get_options SSL_get_options
#define SSL_get_secure_renegotiation_support \
    SSL_get_secure_renegotiation_support
#define SSL_need_tmp_RSA SSL_need_tmp_RSA
#define SSL_num_renegotiations SSL_num_renegotiations
#define SSL_session_reused SSL_session_reused
#define SSL_set0_chain SSL_set0_chain
#define SSL_set1_chain SSL_set1_chain
#define SSL_set1_curves SSL_set1_curves
#define SSL_set_max_cert_list SSL_set_max_cert_list
#define SSL_set_max_send_fragment SSL_set_max_send_fragment
#define SSL_set_mode SSL_set_mode
#define SSL_set_msg_callback_arg SSL_set_msg_callback_arg
#define SSL_set_mtu SSL_set_mtu
#define SSL_set_options SSL_set_options
#define SSL_set_tlsext_host_name SSL_set_tlsext_host_name
#define SSL_set_tmp_dh SSL_set_tmp_dh
#define SSL_set_tmp_ecdh SSL_set_tmp_ecdh
#define SSL_set_tmp_rsa SSL_set_tmp_rsa
#define SSL_total_renegotiations SSL_total_renegotiations

long SSL_CTX_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg) {
	return 0;
}

#ifdef __cplusplus
};
#endif