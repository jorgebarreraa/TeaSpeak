#include <fstream>
#include <cstring>
#include <log/LogUtils.h>
#include <experimental/filesystem>
#include <misc/digest.h>
#include "SSLManager.h"

namespace fs = std::experimental::filesystem;

/*
openssl req -x509 -nodes -out default_certificate.pem -newkey rsa:2048 -keyout default_privatekey.pem -days 3650 -config <(
cat <<-EOF
[req]
default_bits = 2048
prompt = no
default_md = sha256
distinguished_name = dn

[ dn ]
C=DE
O=TeaSpeak
OU=Web Server
emailAddress=contact@teaspeak.de
CN = web.teaspeak.de
EOF
)
 */

/*
static std::string SSL_DEFAULT_KEY = R"(
-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCrnuiIrA/uK/VI
yUDFIc8aClGNzlWvCL18LmhCdE0Mlp0X6aYS/lergGJZark8dNbARNcsU41535L/
0IrqhQr3zsaZqK4sQ5xKtRpUXjknFPMRIcfx7efA/8687yxJ90IJbov7EOT2cVF+
nzE4DNPFyih6G42wMeMTfnuy60fABblWZikeYNM+/YtFcB1uTiJhPWid0URo85d+
e0ifrO7y3EszGd1Hl27oroyFAeGRP5MyU3cj0ZUBjxEdEnJXmAQnHM4cO6tC9E62
775J4G+abJy6TdyxUUK9LhtfTpMnLYpGhMzJwslrfqQ9almY2K7X0xzBYxRNRXsA
B6sBI1llAgMBAAECggEBAJ7pic/j4uxa78jx8XOYFri6DUINaPGmWi5+mjPOlPmv
DM9znj/AG1XGj0rUs6jzV1a5Z7S3uSy8hNUzOS5m+vzzDpqBwqViBXp3r2WnyawS
je+zI/00mX/wXnI71Pq4ZQFux1c3EYvQ6fEhXuXTmtRumIRYtx4LU4RdfhTyH4IB
Rqsb6tPyO0gVPdTL5V3O74bSs0k2QVkm4U/UKiuDJeY5Upy/MX0y+ObEzkxCL/e5
o7jB0DkvCu5wjHWWoA/hduvBXLelbPqxXSuz6YGGPiOaM131Zmk1JIkmeANX1T8b
raWg4yV7oiOprAyx2ioU6Q55w+AYfq2q+noJwEOOrYECgYEA0fJ+pfywksV2eXBu
N2TkruTYsMGE7bSvM8E6ABkYutUz4bC8ytHszs6e3vMhojO6aHqp389vFrPHSu23
1Uqk/eVBbRSHzbvRXl35OzNzeSfmJKkzbG8OnLnukoHzDJUPrhFUHBR8KHnuIycv
87Lj+wmpIaAfaFCct8sKbgnB3e0CgYEA0UQ0GLS6BnwfwdKJVubaIc6P1wtr7vam
UVFntusx3v8AI1qiQ6EzQ73NX9eh3L5sKVcrMtBLur3G/Ferp135J6V2+ZYc1nUA
67x/9wquo42SxlRCfx00zeCCGU0Q56/UEND626So7E6k1sgKnWXq5uuQYZU0wpbi
InD1oo+bulkCgYA2wd2AY2CWV0QoNke40OrIJs3RhBese9S6VepPvjvx9st6UMNc
ztXJtqA/HACosn8q4ttNkWey7x7KjyfETJytz855qcIlyZe42h+37hpu/hYLd8n+
vRR9kg0ETzpaDMKzLrfWPw2G7Q5MQttB32WQwxtGtuGaLnRBh4Zn3smenQKBgANF
DYtVR5LSXaypnXu+H6pnj9fMVeNl9zNOElDJW/4f/eCPifmEi0iDrrHQrLbGQupi
ckpY9tX0ISfQNt5mmX4FF9bOgaTYLyt/xoAVqqTjkWeH6YIS8sBEwcOjcKAuHyIk
IcdMy1bl4613crMC5Ki3BYqAylJACUiAe1YO6GABAoGBALk+VuDvY8IxcSTOZXxe
hGG/TZlHz/xfvuOC0ngsfX+C7Q98KDh72SzBDk4wSqWTLTLPn4jZArmD+gVagNz9
GSb4gpxinfXnb1px0BjR7YoVxJnk8FEjxe7PPSeYWt5e0Liyl0LPBm6xK0LXppyr
N9G+1ojrRslgE5HGdZ9Axi54
-----END PRIVATE KEY-----
)";

static std::string SSL_DEFAULT_CERT = R"(
-----BEGIN CERTIFICATE-----
MIIDYjCCAkoCCQCagl242EEilDANBgkqhkiG9w0BAQsFADBzMQswCQYDVQQGEwJE
RTERMA8GA1UECgwIVGVhU3BlYWsxEzARBgNVBAsMCldlYiBTZXJ2ZXIxIjAgBgkq
hkiG9w0BCQEWE2NvbnRhY3RAdGVhc3BlYWsuZGUxGDAWBgNVBAMMD3dlYi50ZWFz
cGVhay5kZTAeFw0xODAzMjIxNTM4MzVaFw0yODAzMTkxNTM4MzVaMHMxCzAJBgNV
BAYTAkRFMREwDwYDVQQKDAhUZWFTcGVhazETMBEGA1UECwwKV2ViIFNlcnZlcjEi
MCAGCSqGSIb3DQEJARYTY29udGFjdEB0ZWFzcGVhay5kZTEYMBYGA1UEAwwPd2Vi
LnRlYXNwZWFrLmRlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAq57o
iKwP7iv1SMlAxSHPGgpRjc5Vrwi9fC5oQnRNDJadF+mmEv5Xq4BiWWq5PHTWwETX
LFONed+S/9CK6oUK987GmaiuLEOcSrUaVF45JxTzESHH8e3nwP/OvO8sSfdCCW6L
+xDk9nFRfp8xOAzTxcooehuNsDHjE357sutHwAW5VmYpHmDTPv2LRXAdbk4iYT1o
ndFEaPOXfntIn6zu8txLMxndR5du6K6MhQHhkT+TMlN3I9GVAY8RHRJyV5gEJxzO
HDurQvROtu++SeBvmmycuk3csVFCvS4bX06TJy2KRoTMycLJa36kPWpZmNiu19Mc
wWMUTUV7AAerASNZZQIDAQABMA0GCSqGSIb3DQEBCwUAA4IBAQCGNR1J3ynUVwuR
D7RrxfsVYiW/Wx6/i+MQCsk0R+lrsjlkPIUr8iIQu762QszWFaubdh8jqXVu5psT
utQk5RoZ5XrKUcE2av9G4o6Grj3BcsT3JXQcHtIpuDQnJDFoRe950YSVmZKbpvL/
STN46EjSSiDUpv1qqXeVr9CEyCZftj4esJ6RvJwYeKBG8HXoNzMYK32N6JWGbZFu
U76TSNwcjXNId43V4OVFZ/ReaD8Nvzq10GwgKb2HshQtdIvOVNfAAk/mX4e+5I1k
9zTEm+IBljBFvNzAKQRxUiiUDTjazKVt51ToJhRVBGXtPmVvhKacWWC3tbHdWisG
5vm7hxLQ
-----END CERTIFICATE-----
)";
*/

using namespace ts;
using namespace ts::ssl;
using namespace std;

SSLManager::SSLManager() = default;
SSLManager::~SSLManager() = default;

bool SSLManager::initialize() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    return true;
}

static auto ERR_TO_STRING = [](const char* err, size_t length, void* ptrTarget){
    auto target = (string*) ptrTarget;
    target->resize(length);
    memcpy((void *) target->data(), err, length);
    return 0;
};

#define SSL_ERROR(message)                                  \
do {                                                        \
    ERR_print_errors_cb(ERR_TO_STRING, &error);             \
    error = (message) + error;                              \
    return nullptr;                                         \
} while(false)

std::shared_ptr<SSLContext> SSLManager::initializeContext(const std::string &context, std::string &privateKey, std::string &certificate, std::string &error, bool raw, const std::shared_ptr<SSLGenerator>& generator) {
    auto load = this->loadContext(privateKey, certificate, error, raw, generator);
    if(!load) return nullptr;

    {
        lock_guard lock{this->context_lock};
        this->contexts[context] = load;
    }
    if(context.find(this->web_ctx_prefix) == 0) {
        lock_guard lock{this->_web_options_lock};
        this->_web_options.reset();
    }
    return load;
}

std::shared_ptr<SSLKeyPair> SSLManager::initializeSSLKey(const std::string &key, const std::string &rsaKey, std::string &error, bool raw) {
    auto load = this->loadSSL(rsaKey, error, raw);
    if(!load) return nullptr;

    {
        lock_guard lock{this->context_lock};
        this->rsa[key] = load;
    }
    return load;
}

bool SSLManager::rename_context(const std::string &old_name, const std::string &new_name) {
    {
        lock_guard lock{this->context_lock};
        if(this->contexts.count(old_name) == 0)
            return false;
        auto old = std::move(this->contexts[old_name]);
        this->contexts.erase(old_name);
        this->contexts[new_name] = std::move(old);
    }

    if(old_name.find(this->web_ctx_prefix) == 0 || new_name.find(this->web_ctx_prefix) == 0) {
        lock_guard lock{this->_web_options_lock};
        this->_web_options.reset();
    }
    return true;
}

bool SSLManager::unregister_context(const std::string &context) {
    {
        lock_guard lock{this->context_lock};
        if(this->contexts.erase(context) == 0)
            return false;
    }
    if(context.find(this->web_ctx_prefix) == 0) {
        lock_guard lock{this->_web_options_lock};
        this->_web_options.reset();
    }
    return true;
}

void SSLManager::unregister_web_contexts(bool default_as_well) {
    {
        lock_guard lock{this->context_lock};
        decltype(this->contexts) ctxs{this->contexts};
        for(auto& [key, _] : ctxs) {
            if(key == this->web_ctx_prefix + "default" && !default_as_well)
                continue;

            (void) _;
            if(key.find(this->web_ctx_prefix) == 0) {
                this->contexts.erase(key);
            }
        }
    }

    lock_guard lock{this->_web_options_lock};
    this->_web_options.reset();
}

EVP_PKEY* SSLGenerator::generateKey() {
    auto key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(EVP_PKEY_new(), ::EVP_PKEY_free);

    auto rsa = RSA_new();
    auto e = std::unique_ptr<BIGNUM, decltype(&BN_free)>(BN_new(), ::BN_free);
    BN_set_word(e.get(), RSA_F4);
    if(!RSA_generate_key_ex(rsa, 2048, e.get(), nullptr)) return nullptr;
    EVP_PKEY_assign_RSA(key.get(), rsa);
    return key.release();
}

X509* SSLGenerator::generateCertificate(EVP_PKEY* key) {

    auto cert = X509_new();
    X509_set_pubkey(cert, key);

    ASN1_INTEGER_set(X509_get_serialNumber(cert), 3);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 31536000L);

    X509_NAME* name = nullptr;
    name = X509_get_subject_name(cert);
    for(const auto& subject : this->subjects)
        X509_NAME_add_entry_by_txt(name, subject.first.c_str(),  MBSTRING_ASC, (unsigned char *) subject.second.c_str(), subject.second.length(), -1, 0);
    X509_set_subject_name(cert, name);

    name = X509_get_issuer_name(cert);
    for(const auto& subject : this->issues)
        X509_NAME_add_entry_by_txt(name, subject.first.c_str(),  MBSTRING_ASC, (unsigned char *) subject.second.c_str(), subject.second.length(), -1, 0);

    X509_set_issuer_name(cert, name);

    X509_sign(cert, key, EVP_sha512());
    return cert;
}

//TODO passwords
std::shared_ptr<SSLContext> SSLManager::loadContext(std::string &rawKey, std::string &rawCert, std::string &error, bool is_raw, const shared_ptr<SSLGenerator>& generator) {
    std::shared_ptr<BIO> bio_certificate = nullptr;
    std::shared_ptr<BIO> bio_private_key = nullptr;
    std::shared_ptr<X509> certificate = nullptr;
    std::shared_ptr<EVP_PKEY> key = nullptr;
    bool allow_generate_cert{false}, allow_generate_key{false};
    bool certificate_modified{false}, key_modified{false};
    std::shared_ptr<SSL_CTX> context = nullptr;
    std::shared_ptr<SSLContext> result = nullptr;

    if(is_raw) {
        if(!rawKey.empty()) {
            bio_private_key = shared_ptr<BIO>(BIO_new_mem_buf(rawKey.data(), rawKey.length()), ::BIO_free);

            if(!rawCert.empty()) {
                bio_certificate = shared_ptr<BIO>(BIO_new_mem_buf(rawCert.data(), rawCert.length()), ::BIO_free);
            } else {
                allow_generate_cert = true;
            }
        } else {
            allow_generate_cert = true;
            allow_generate_key = true;
        }
        if(!bio_certificate) bio_certificate = shared_ptr<BIO>(BIO_new(BIO_s_mem()), ::BIO_free);
        if(!bio_private_key) bio_private_key = shared_ptr<BIO>(BIO_new(BIO_s_mem()), ::BIO_free);
    } else {
        auto key_path = fs::u8path(rawKey);
        auto certificate_path = fs::u8path(rawCert);

        auto key_exists = fs::exists(key_path);
        auto certificate_exists = fs::exists(certificate_path);

        if(!key_exists) {
            try {
                if(key_path.has_parent_path())
                    fs::create_directories(key_path.parent_path());
            } catch (fs::filesystem_error& ex) {
                error = "failed to create keys file parent path: " + std::string{ex.what()};
                return nullptr;
            }

            std::ofstream {key_path};
            allow_generate_key = true;
        }

        if(!certificate_exists) {
            try {
                if(certificate_path.has_parent_path())
                    fs::create_directories(certificate_path.parent_path());
            } catch (fs::filesystem_error& ex) {
                error = "failed to create certificate file parent path: " + std::string{ex.what()};
                return nullptr;
            }

            std::ofstream {certificate_path};
            allow_generate_cert = true;
        } else if(!key_exists) {
            error = "missing private key";
            return nullptr;
        }

        {
            auto mode = allow_generate_cert ? "rw" : "r";
            bio_certificate = shared_ptr<BIO>(BIO_new_file(rawCert.c_str(), mode), ::BIO_free);
            if(!bio_certificate) SSL_ERROR("Could not open certificate: ");
        }

        {
            auto mode = allow_generate_key ? "rw" : "r";
            bio_private_key = shared_ptr<BIO>(BIO_new_file(rawKey.c_str(), mode), ::BIO_free);
            if(!bio_private_key) SSL_ERROR("Could not open key: ");
        }
    }


    certificate = shared_ptr<X509>(PEM_read_bio_X509(bio_certificate.get(), nullptr, nullptr, nullptr), ::X509_free);
    if(!certificate && (!generator || !allow_generate_cert)) SSL_ERROR("Could not read certificate: ");

    key = shared_ptr<EVP_PKEY>(PEM_read_bio_PrivateKey(bio_private_key.get(), nullptr, nullptr, nullptr), ::EVP_PKEY_free);
    if(!key && (!generator || !allow_generate_key)) SSL_ERROR("Could not read key: ");

    if(!key) {
        key = shared_ptr<EVP_PKEY>(generator->generateKey(), ::EVP_PKEY_free);
        key_modified = true;
    }

    if(!certificate) {
        certificate = shared_ptr<X509>(generator->generateCertificate(key.get()), ::X509_free);
        certificate_modified = true;
    }

    //Create context
    context = shared_ptr<SSL_CTX>(SSL_CTX_new(SSLv23_server_method()), ::SSL_CTX_free);
    if (!context) SSL_ERROR("Could not create context: ");


    if (SSL_CTX_use_PrivateKey(context.get(), key.get()) <= 0) SSL_ERROR("Could not use private key: ");
    if (SSL_CTX_use_certificate(context.get(), certificate.get()) <= 0) SSL_ERROR("Could not use certificate: ");

    result = std::make_shared<SSLContext>();
    result->context = context;
    result->certificate = certificate;
    result->privateKey = key;

    if(key_modified) {
        if(!is_raw) {
            bio_private_key = shared_ptr<BIO>(BIO_new_file(rawKey.c_str(), "w"), ::BIO_free);
            if(PEM_write_bio_PrivateKey(bio_private_key.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1) SSL_ERROR("Could not write new key: ");
        } else {
            bio_private_key = shared_ptr<BIO>(BIO_new(BIO_s_mem()), ::BIO_free);
            if(PEM_write_bio_PrivateKey(bio_private_key.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1) SSL_ERROR("Could not write new key: ");

            const uint8_t* mem_ptr{nullptr};
            size_t length{0};
#ifdef CRYPTO_BORINGSSL
            if(!BIO_mem_contents(&*bio_private_key, &mem_ptr, &length)) SSL_ERROR("Failed to get mem contents: ");
#else
            BUF_MEM* memory{nullptr};
            if(!BIO_get_mem_ptr(&*bio_private_key, &memory) || !memory) SSL_ERROR("Failed to get mem contents: ");

            mem_ptr = (uint8_t*) memory->data;
            length = memory->length;
#endif
            if(!mem_ptr || length < 0) SSL_ERROR("Could not get private key mem pointer/invalid length: ");
            rawKey.reserve(length);
            memcpy(rawKey.data(), mem_ptr, length);
        }
    }

    if(certificate_modified) {
        if(!is_raw) {
            bio_certificate = shared_ptr<BIO>(BIO_new_file(rawCert.c_str(), "w"), ::BIO_free);
            if(PEM_write_bio_X509(bio_certificate.get(), certificate.get()) != 1) SSL_ERROR("Could not write new certificate: ");
        } else {
            bio_certificate = shared_ptr<BIO>(BIO_new(BIO_s_mem()), ::BIO_free);
            if(PEM_write_bio_X509(bio_certificate.get(), certificate.get()) != 1) SSL_ERROR("Could not write new certificate: ");

            const uint8_t* mem_ptr{nullptr};
            size_t length{0};
#ifdef CRYPTO_BORINGSSL
            if(!BIO_mem_contents(&*bio_private_key, &mem_ptr, &length)) SSL_ERROR("Failed to get mem contents: ");
#else
            BUF_MEM* memory{nullptr};
            if(!BIO_get_mem_ptr(&*bio_private_key, &memory) || !memory) SSL_ERROR("Failed to get mem contents: ");

            mem_ptr = (uint8_t*) memory->data;
            length = memory->length;
#endif
            if(!mem_ptr || length < 0) SSL_ERROR("Could not get cert bio mem pointer/invalid length: ");
            rawCert.reserve(length);
            memcpy(rawCert.data(), mem_ptr, length);
        }
    }

    return result;
}

std::shared_ptr<SSLKeyPair> SSLManager::loadSSL(const std::string &key_data, std::string &error, bool rawData, bool readPublic) {
    std::shared_ptr<BIO> key_bio{nullptr};
    std::shared_ptr<EVP_PKEY> key{nullptr};
    auto result = make_shared<SSLKeyPair>();
//    SSL_CTX_set_ecdh_auto(ctx, 1);
    if(rawData) {
        key_bio = shared_ptr<BIO>(BIO_new(BIO_s_mem()), ::BIO_free);
        BIO_write(key_bio.get(), key_data.c_str(), key_data.length());
    } else {
        auto key_path = fs::u8path(key_data);

        if(!fs::exists(key_path)) {
            try {
                if(key_path.has_parent_path())
                    fs::create_directories(key_path.parent_path());
            } catch (fs::filesystem_error& error) {
                logError(LOG_GENERAL, "Could not create key directory: " + string(error.what()));
            }

            {
                std::ofstream { key_path };
            }
        }
        key_bio = shared_ptr<BIO>(BIO_new_file(key_data.c_str(), "r"), ::BIO_free);
        if(!key_bio) SSL_ERROR("Could not load key: ");
    }

    if(readPublic)
        key = shared_ptr<EVP_PKEY>(PEM_read_bio_PUBKEY(key_bio.get(), nullptr, nullptr, nullptr), ::EVP_PKEY_free);
    else
        key = shared_ptr<EVP_PKEY>(PEM_read_bio_PrivateKey(key_bio.get(), nullptr, nullptr, nullptr), ::EVP_PKEY_free);
    result->contains_private = !readPublic;
    if(!key) {
        if(readPublic) {
            SSL_ERROR("Could not read key!");
        } else return this->loadSSL(key_data, error, rawData, true);
    }

    result->key = key;
    return result;
}

bool SSLManager::verifySign(const std::shared_ptr<SSLKeyPair> &key, const std::string &message, const std::string &sign) {
    assert(key);
    auto hash = digest::sha256(message);
    return RSA_verify(NID_sha256, (u_char*) hash.data(), hash.length(), (u_char*) sign.data(), sign.length(), EVP_PKEY_get1_RSA(key->key.get())) == 1;
}

std::shared_ptr<pipes::SSL::Options> SSLManager::web_ssl_options() {
    lock_guard lock(this->_web_options_lock);
    if(this->_web_options || this->_web_disabled)
        return this->_web_options;

    this->_web_options = make_shared<pipes::SSL::Options>();
    this->_web_options->type = pipes::SSL::SERVER;
    this->_web_options->context_method = TLS_method();
    this->_web_options->free_unused_keypairs = false; /* we dont want our keys get removed */


    lock_guard ctx_lock{this->context_lock};
    for(auto& context : this->contexts) {
        auto name = context.first;
        if(name.length() < this->web_ctx_prefix.length())
            continue;
        if(name.substr(0, this->web_ctx_prefix.length()) != this->web_ctx_prefix)
            continue;

        auto servername = context.first.substr(this->web_ctx_prefix.length());
        if(servername == "default") {
            this->_web_options->default_keypair({context.second->privateKey, context.second->certificate});
        } else {
            this->_web_options->servername_keys[servername] = {context.second->privateKey, context.second->certificate};
        }
    }
    return this->_web_options;
}