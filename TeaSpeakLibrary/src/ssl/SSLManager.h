#pragma once

#include "openssl/ssl.h"
#include "openssl/err.h"
#include <Definitions.h>
#include <map>
#include <pipes/ssl.h>

namespace ts::ssl {
        struct SSLContext {
            std::shared_ptr<SSL_CTX> context{nullptr};
            std::shared_ptr<EVP_PKEY> privateKey{nullptr};
            std::shared_ptr<X509> certificate{nullptr};
        };

        struct SSLGenerator {
            std::deque<std::pair<std::string, std::string>> subjects;
            std::deque<std::pair<std::string, std::string>> issues;

            EVP_PKEY* generateKey();
            X509* generateCertificate(EVP_PKEY*);
        };

        struct SSLKeyPair {
            bool contains_private = false;
            std::shared_ptr<EVP_PKEY> key = nullptr;
        };

        class SSLManager {
            public:
                SSLManager();
                virtual ~SSLManager();

                bool initialize();
                void printDetails();

                bool unregister_context(const std::string& /* key */);
                bool rename_context(const std::string& /* old key */, const std::string& /* new key */); /* if new already exists it will be dropped */
                void unregister_web_contexts(bool /* default certificate as well */);

                std::shared_ptr<SSLKeyPair> initializeSSLKey(const std::string &key, const std::string &rsaKey, std::string &error, bool raw = false);
                std::shared_ptr<SSLContext> initializeContext(const std::string& key, std::string& privateKey, std::string& certificate, std::string& error, bool raw = false, const std::shared_ptr<SSLGenerator>& = nullptr);

                std::shared_ptr<SSLContext> getContext(const std::string& key){ return this->contexts[key]; }
                std::shared_ptr<SSLKeyPair> getRsaKey(const std::string& key){ return this->rsa[key]; }

                bool verifySign(const std::shared_ptr<SSLKeyPair>& key, const std::string& message, const std::string& sign);

                void disable_web() { this->_web_disabled = true; }
                std::shared_ptr<pipes::SSL::Options> web_ssl_options();
                std::shared_ptr<SSLContext> getQueryContext() { return this->getContext("query"); }

            private:
                std::mutex context_lock{};
                std::map<std::string, std::shared_ptr<SSLContext>> contexts;
                std::map<std::string, std::shared_ptr<SSLKeyPair>> rsa;

                const std::string web_ctx_prefix{"web_"};
                std::mutex _web_options_lock{};
                bool _web_disabled{false};
                std::shared_ptr<pipes::SSL::Options> _web_options;

                std::shared_ptr<SSLContext> loadContext(std::string& rawKey, std::string& rawCert, std::string& error, bool rawData = false, const std::shared_ptr<SSLGenerator>& = nullptr);
                std::shared_ptr<SSLKeyPair> loadSSL(const std::string &key_data, std::string &error, bool rawData = false, bool readPublic = false);
        };
    }