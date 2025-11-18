//
// Created by WolverinDEV on 29/04/2020.
//

#include <files/FileServer.h>
#include <log/LogUtils.h>

#include <experimental/filesystem>
#include <local_server/clnpath.h>
#include <event2/thread.h>
#include <include/files/Config.h>
#include <local_server/HTTPUtils.h>

namespace fs = std::experimental::filesystem;

using namespace ts::server;

struct Nothing {};

template <typename ErrorType, typename ResponseType>
inline void print_fs_response(const std::string& message, const std::shared_ptr<file::ExecuteResponse<file::filesystem::DetailedError<ErrorType>, ResponseType>>& response) {
    if(response->status == file::ExecuteStatus::ERROR)
        logError(LOG_FT, "{}: {} => {}", message, (int) response->error().error_type, response->error().error_message);
    else if(response->status == file::ExecuteStatus::SUCCESS)
        logMessage(LOG_FT, "{}: success", message);
    else
        logWarning(LOG_FT, "Unknown response state ({})!", (int) response->status);
}

template <typename ErrorType, typename ResponseType>
inline void print_ft_response(const std::string& message, const std::shared_ptr<file::ExecuteResponse<ErrorType, ResponseType>>& response) {
    if(response->status == file::ExecuteStatus::ERROR)
        logError(LOG_FT, "{}: {} => {}", message, (int) response->error().error_type, response->error().error_message);
    else if(response->status == file::ExecuteStatus::SUCCESS)
        logMessage(LOG_FT, "{}: success", message);
    else
        logWarning(LOG_FT, "Unknown response state ({})!", (int) response->status);
}

inline void print_query(const std::string& message, const file::filesystem::AbstractProvider::directory_query_response_t& response) {
    if(response.status == file::ExecuteStatus::ERROR)
        logError(LOG_FT, "{}: {} => {}", message, (int) response.error().error_type, response.error().error_message);
    else if(response.status == file::ExecuteStatus::SUCCESS) {
        const auto& entries = response.response();
        logMessage(LOG_FT, "{}: Found {} entries", message, entries.size());
        for(auto& entry : entries) {
            if(entry.type == file::filesystem::DirectoryEntry::FILE)
                logMessage(LOG_FT, "  - File {}", entry.name);
            else if(entry.type == file::filesystem::DirectoryEntry::DIRECTORY)
                logMessage(LOG_FT, "  - Directory {}", entry.name);
            else
                logMessage(LOG_FT, "  - Unknown {}", entry.name);
            logMessage(LOG_FT, "    Write timestamp: {}", std::chrono::floor<std::chrono::seconds>(entry.modified_at.time_since_epoch()).count());
            logMessage(LOG_FT, "    Size: {}", entry.size);
        }
    } else
        logWarning(LOG_FT, "{}: Unknown response state ({})!", message, (int) response.status);
}

EVP_PKEY* ssl_generate_key() {
    auto key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(EVP_PKEY_new(), ::EVP_PKEY_free);

    auto rsa = RSA_new();
    auto e = std::unique_ptr<BIGNUM, decltype(&BN_free)>(BN_new(), ::BN_free);
    BN_set_word(e.get(), RSA_F4);
    if(!RSA_generate_key_ex(rsa, 2048, e.get(), nullptr)) return nullptr;
    EVP_PKEY_assign_RSA(key.get(), rsa);
    return key.release();
}

X509* ssl_generate_certificate(EVP_PKEY* key) {
    auto cert = X509_new();
    X509_set_pubkey(cert, key);

    ASN1_INTEGER_set(X509_get_serialNumber(cert), 3);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 31536000L);

    X509_NAME* name = nullptr;
    name = X509_get_subject_name(cert);
    //for(const auto& subject : this->subjects)
    //    X509_NAME_add_entry_by_txt(name, subject.first.c_str(),  MBSTRING_ASC, (unsigned char *) subject.second.c_str(), subject.second.length(), -1, 0);
    X509_set_subject_name(cert, name);

    name = X509_get_issuer_name(cert);
    //for(const auto& subject : this->issues)
    //    X509_NAME_add_entry_by_txt(name, subject.first.c_str(),  MBSTRING_ASC, (unsigned char *) subject.second.c_str(), subject.second.length(), -1, 0);

    X509_set_issuer_name(cert, name);

    X509_sign(cert, key, EVP_sha512());
    return cert;
}

int main() {
    evthread_use_pthreads();
    {
        std::map<std::string, std::string> query{};
        http::parse_url_parameters("http://www.example.org/suche?stichwort=wiki&no-arg&1arg=&ausgabe=liste&test=test#bla=d&blub=c", query);
        for(const auto& [key, value] : query)
            std::cout << key << " => " << value << std::endl;
        return 0;
    }

    auto log_config = std::make_shared<logger::LoggerConfig>();
    log_config->terminalLevel = spdlog::level::trace;
    logger::setup(log_config);

    std::string error{};
    if(!file::initialize(error)) {
        logError(LOG_FT, "Failed to initialize file server: {}", error);
        return 0;
    }
    logMessage(LOG_FT, "File server started");
    auto instance = file::server();


    {
        auto options = std::make_shared<pipes::SSL::Options>();
        options->verbose_io = true;
        options->context_method = SSLv23_method();
        options->free_unused_keypairs = false;

        {
            std::shared_ptr<EVP_PKEY> pkey{ssl_generate_key(), ::EVP_PKEY_free};
            std::shared_ptr<X509> cert{ssl_generate_certificate(&*pkey), ::X509_free};

            options->default_keypair({pkey, cert});
        }
        file::config::ssl_option_supplier = [options]{
            return options;
        };
    }

#if 0
    auto& fs = instance->file_system();
    {
        auto response = fs.initialize_server(0);
        response->wait();
        print_fs_response("Server init result", response);
        if(response->status != file::ExecuteStatus::SUCCESS)
            return 0;
    }


    {
        auto response = fs.create_channel_directory(0, 2, "/");
        response->wait();
        print_fs_response("Channel dir create A", response);
    }


    {
        auto response = fs.create_channel_directory(0, 2, "/test-folder/");
        response->wait();
        print_fs_response("Channel dir create B", response);
    }


    {
        auto response = fs.create_channel_directory(0, 2, "../test-folder/");
        response->wait();
        print_fs_response("Channel dir create C", response);
    }


    {
        auto response = fs.create_channel_directory(0, 2, "./test-folder/../test-folder-2");
        response->wait();
        print_fs_response("Channel dir create D", response);
    }

    {
        auto response = fs.query_channel_directory(0, 2, "/");
        response->wait();
        print_query("Channel query", *response);
    }

    {
        auto response = fs.query_icon_directory(0);
        response->wait();
        print_query("Icons", *response);
    }

    {
        auto response = fs.query_avatar_directory(0);
        response->wait();
        print_query("Avatars", *response);
    }

    {
        auto response = fs.rename_channel_file(0, 2, "./test-folder/../test-folder-2", "./test-folder/../test-folder-3");
        response->wait();
        print_fs_response("Folder rename A", response);
    }

    {
        auto response = fs.rename_channel_file(0, 2, "./test-folder/../test-folder-3", "./test-folder/../test-folder-2");
        response->wait();
        print_fs_response("Folder rename B", response);
    }
#endif

#if 0
    auto& ft = instance->file_transfer();

    ft.callback_transfer_finished = [](const std::shared_ptr<file::transfer::Transfer>& transfer) {
        logMessage(0, "Transfer finished");
    };

    ft.callback_transfer_started = [](const std::shared_ptr<file::transfer::Transfer>& transfer) {
        logMessage(0, "Transfer started");
    };

    ft.callback_transfer_aborted = [](const std::shared_ptr<file::transfer::Transfer>& transfer, const transfer::TransferStatistics&, const file::transfer::TransferError& error) {
        logMessage(0, "Transfer aborted ({}/{})", (int) error.error_type, error.error_message);
    };

    ft.callback_transfer_statistics = [](const std::shared_ptr<file::transfer::Transfer>& transfer, const file::transfer::TransferStatistics& stats) {
        logMessage(0, "Transfer stats. New file bytes: {}, delta bytes send {}", stats.delta_file_bytes_transferred, stats.delta_network_bytes_send);
    };

    {
        auto response = ft.initialize_channel_transfer(file::transfer::Transfer::DIRECTION_UPLOAD, 0, 2, {
                "test2.txt",
                false,
                4,
                120,
                32
        });
        response->wait();
        print_ft_response("Download test.txt", response);
        if(response->succeeded())
            logMessage(LOG_FT, "Download key: {}", std::string{response->response()->transfer_key, TRANSFER_KEY_LENGTH});
    }
#endif

    std::this_thread::sleep_for(std::chrono::seconds{120});
    //TODO: Test file locking
    file::finalize();
    return 0;
}