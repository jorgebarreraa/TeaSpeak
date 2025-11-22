#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <cstring>
#include <iostream>
#include <deque>
#include <ThreadPool/Mutex.h>
#include <ThreadPool/Thread.h>
#include <src/log/LogUtils.h>
#include <src/ssl/SSLSocket.h>
#include <event.h>
#include <src/ws/WebSocket.h>

using namespace std;
int create_socket(int port)
{
    int s;
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }
    int optval = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0)
        printf("Cannot set SO_REUSEADDR option on listen socket (%s)\n", strerror(errno));

    if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(int)) < 0)
        printf("Cannot set SO_REUSEADDR option on listen socket (%s)\n", strerror(errno));

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Unable to bind");
        exit(EXIT_FAILURE);
    }
    if (listen(s, 1) < 0) {
        perror("Unable to listen");
        exit(EXIT_FAILURE);
    }

    return s;
}

void setNonBlock(int fd) {

}

void init_openssl()
{
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl()
{
    EVP_cleanup();
}

SSL_CTX *create_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

//#define CERTIFICATE_FILE "cert.pem"
//#define KEY_FILE "key.pem"
#define CERTIFICATE_FILE "/home/wolverindev/TeamSpeak/server/environment/default_certificate.pem"
#define KEY_FILE "/home/wolverindev/TeamSpeak/server/environment/default_privatekey.pem"

std::pair<EVP_PKEY*, X509*> createCerts(pem_password_cb* password) {
/*
    auto bio = BIO_new_file("cert.pem", "r");
    if(!bio) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    auto cert = PEM_read_bio_X509(bio, nullptr, password, nullptr);
    if(!cert) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    BIO_free(bio);


    bio = BIO_new_file("key.pem", "r");
    if(!bio) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    auto key = PEM_read_bio_PrivateKey(bio, nullptr, password, nullptr);

    if(!key) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    BIO_free(bio);
*/



    auto key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(EVP_PKEY_new(), ::EVP_PKEY_free);

    auto rsa = RSA_new();
    auto e = std::unique_ptr<BIGNUM, decltype(&BN_free)>(BN_new(), ::BN_free);
    BN_set_word(e.get(), RSA_F4);
    if(!RSA_generate_key_ex(rsa, 2048, e.get(), nullptr)) return {nullptr, nullptr};
    EVP_PKEY_assign_RSA(key.get(), rsa);

    auto cert = X509_new();
    X509_set_pubkey(cert, key.get());

    ASN1_INTEGER_set(X509_get_serialNumber(cert), 3);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 31536000L);

    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC, (unsigned char *) "DE", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC, (unsigned char *) "TeaSpeak", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "OU",  MBSTRING_ASC, (unsigned char *) "Web Server", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "emailAddress",  MBSTRING_ASC, (unsigned char *)"contact@teaspeak.de", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)"web.teaspeak.de", -1, -1, 0);

    X509_set_issuer_name(cert, name);
    X509_set_subject_name(cert, name);

    X509_sign(cert, key.get(), EVP_sha512());



    return {key.release(), cert};
};

/*
 * Generate key:

openssl req -x509 -out cert.pem -newkey rsa:2048 -keyout key.pem -days 365 -passout pass:markus -config <(
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

void configure_context(SSL_CTX *ctx)
{
    SSL_CTX_set_ecdh_auto(ctx, 1);
    auto certs = createCerts([](char* buffer, int length, int rwflag, void* data) -> int {
        std::string password = "markus";
        memcpy(buffer, password.data(), password.length());
        return password.length();
    });

    PEM_write_X509(stdout, certs.second);

    if (SSL_CTX_use_PrivateKey(ctx, certs.first) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate(ctx, certs.second) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

}

struct Client {
    int fd;
    ssl::SSLSocket ssl;
    ws::WebSocket ws;

    std::deque<std::string> writeQueue;

    event* read;
    event* write;
};

void handleRead(int fd, short, void* ptrClient) {
    auto client = (Client*) ptrClient;

    ssize_t bufferLength = 1024;
    char buffer[bufferLength];

    bufferLength = recv(client->fd, buffer, bufferLength, MSG_DONTWAIT);
    if(bufferLength < 0){
        cout << "Invalid read: " << bufferLength << " / " << errno << endl;
        event_del(client->read);
        event_del(client->write);
        return;
    } else if(bufferLength == 0) return;

    cout << "Read " << bufferLength << " bytes" << endl;
    client->ssl.proceedMessage(string(buffer, bufferLength));
}

void handleWrite(int fd, short, void* ptrClient) {
    auto client = (Client*) ptrClient;
    while(!client->writeQueue.empty()) {
        auto message = std::move(client->writeQueue.front());
        client->writeQueue.pop_front();
        send(client->fd, message.data(), message.length(), 0);
        cout << "Send " << message.length() << " bytes" << endl;
    }
}

int main(int argc, char **argv)
{
    int sock;
    SSL_CTX *ctx;

    init_openssl();
    ctx = create_context();
    configure_context(ctx);

    sock = create_socket(4433);

    auto evLoop = event_base_new();
    threads::Thread([evLoop](){
        while(true) {
            event_base_dispatch(evLoop);
            threads::self::sleep_for(std::chrono::milliseconds(10));
        };
        cerr << "event_base_dispatch() exited!" << endl;
    }).detach();
    /* Handle connections */
    cout << "Waiting!" << endl;
    while(1) {
        struct sockaddr_in addr{};
        uint len = sizeof(addr);

        int fd = accept(sock, (struct sockaddr*)&addr, &len);
        if (fd < 0) {
            perror("Unable to accept");
            exit(EXIT_FAILURE);
        }
        auto client = new Client{};
        client->fd = fd;


        client->read = event_new(evLoop, fd, EV_READ | EV_PERSIST, handleRead, client);
        client->write = event_new(evLoop, fd, EV_WRITE, handleWrite, client);

        client->ssl.callback_error = [](ssl::SSLSocketError, const std::string& error) {
            cout << error << endl;
        };
        client->ssl.callback_write = [client](const std::string& buffer) {
            client->writeQueue.push_back(buffer);
            event_add(client->write, nullptr);
        };
        client->ssl.callback_read = [client](const std::string& buffer) {
            cout << buffer;
            client->ws.proceedMessage(buffer);
        };
        auto ptr = std::shared_ptr<SSL_CTX>(ctx, [](SSL_CTX*){});
        client->ssl.initialize(ptr);

        client->ws.on_message = [](const std::string& message) {
            cout << "[WS] Message: " << endl << message << endl;
        };
        client->ws.write_message = [client](const std::string& data) {
            cout << "[WS] Send: " << endl << data << endl;
            client->ssl.sendMessage(data);
        };

        client->ws.initialize();

        event_add(client->read, nullptr);
    }

    close(sock);
    SSL_CTX_free(ctx);
    cleanup_openssl();
}