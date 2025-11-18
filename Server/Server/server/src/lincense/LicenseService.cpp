//
// Created by WolverinDEV on 27/02/2020.
//
#include <netdb.h>
#include <cassert>
#include <misc/strobf.h>
#include <log/LogUtils.h>
#include <src/Configuration.h>
#include <src/ShutdownHelper.h>
#include <license/client.h>
#include "src/InstanceHandler.h"
#include "LicenseRequest.pb.h"

//#define DO_LOCAL_REQUEST
using namespace ts::server::license;

LicenseService::LicenseService() {
    this->dns.lock = std::make_shared<std::recursive_mutex>();
}

LicenseService::~LicenseService() {
    {
        std::lock_guard lock{this->request_lock};
        this->abort_request(lock, "");
    }
}

bool LicenseService::initialize(std::string &error) {
    //this->verbose_ = true;
    this->startup_timepoint_ = std::chrono::steady_clock::now();
    this->timings.next_request = std::chrono::system_clock::now() + std::chrono::seconds(rand() % 20);
    return true;
}

bool LicenseService::execute_request_sync(const std::chrono::milliseconds& timeout) {
    std::unique_lock slock{this->sync_request_lock};
    this->begin_request();

    if(this->sync_request_cv.wait_for(slock, timeout) == std::cv_status::timeout)
        return false;

    return this->timings.failed_count == 0;
}

void LicenseService::shutdown() {
    std::lock_guard lock{this->request_lock};
    if(this->request_state_ == request_state::empty) return;

    this->abort_request(lock, "shutdown");
}

void LicenseService::begin_request() {
    std::lock_guard lock{this->request_lock};
    if(this->request_state_ != request_state::empty)
        this->abort_request(lock, "last request has been aborted");

    if(this->verbose_)
        debugMessage(LOG_INSTANCE, strobf("Executing license request.").string());
    this->timings.last_request = std::chrono::system_clock::now();
    this->request_state_ = request_state::dns_lookup;
    this->execute_dns_request();
}

void LicenseService::abort_request(std::lock_guard<std::recursive_timed_mutex> &, const std::string& reason) {
    if(this->request_state_ == request_state::dns_lookup) {
        this->abort_dns_request();
        return;
    } else if(this->current_client) {
        this->current_client->callback_connected = nullptr;
        this->current_client->callback_message = nullptr;
        this->current_client->callback_disconnected = nullptr;

        if(!reason.empty()) {
            this->current_client->disconnect(reason, std::chrono::system_clock::now() + std::chrono::seconds{1});
            /* Lets not wait here because we might be within the event loop. */
            //if(!this->current_client->await_disconnect())
            //    this->current_client->close_connection();
        } else {
            this->current_client->close_connection();
        }

        this->current_client.reset();
    }
}

void LicenseService::abort_dns_request() {
    std::unique_lock llock{*this->dns.lock};
    if(!this->dns.current_lookup) return;

    this->dns.current_lookup->handle = nullptr;
    this->dns.current_lookup = nullptr;
}

void LicenseService::execute_dns_request() {
    std::unique_lock llock{*this->dns.lock};
    assert(!this->dns.current_lookup);

    auto lookup = new _dns::_lookup{};

    lookup->lock = this->dns.lock;
    lookup->handle = this;
    lookup->thread = std::thread([lookup] {
        bool success{false};
        std::string error{};
        sockaddr_in server_addr{};

        {
            server_addr.sin_family = AF_INET;
#ifdef DO_LOCAL_REQUEST
            auto license_host = gethostbyname(strobf("localhost").c_str());
#else
            auto license_host = gethostbyname(strobf("license.teamspeak.cl").c_str());
#endif
            if(!license_host) {
                error = strobf("resolve error occurred: ").string() + hstrerror(h_errno);
                goto handle_result;
            }
            if(!license_host->h_addr){
                error = strobf("missing h_addr in result").string();
                goto handle_result;
            }

            server_addr.sin_addr.s_addr = ((in_addr*) license_host->h_addr)->s_addr;

#ifndef DO_LOCAL_REQUEST
            int first = server_addr.sin_addr.s_addr >> 24;
            if(first == 0 || first == 127 || first == 255) {
                error = strobf("local response address").string();
                goto handle_result;
            }
#endif
            server_addr.sin_port = htons(27786);
            success = true;
        }

        handle_result:
        {
            std::unique_lock llock{*lookup->lock};
            if(lookup->handle) {
                lookup->handle->dns.current_lookup = nullptr;

                if(success) {
                    debugMessage(LOG_INSTANCE, strobf("Successfully resolved the hostname to {}").string(), net::to_string(server_addr.sin_addr));
                    lookup->handle->handle_dns_lookup_result(true, server_addr);
                } else {
                    debugMessage(LOG_INSTANCE, strobf("Failed to resolve hostname for license server: {}").string(), error);
                    lookup->handle->handle_dns_lookup_result(false, error);
                }
            }

            assert(lookup->thread.get_id() == std::this_thread::get_id());
            if(lookup->thread.joinable())
                lookup->thread.detach();
            delete lookup;
        }
    });


    this->dns.current_lookup = lookup;
}

void LicenseService::handle_check_succeeded() {
    this->timings.last_succeeded = std::chrono::system_clock::now();
    {
        std::lock_guard rlock{this->request_lock};
        this->abort_request(rlock, strobf("request succeeded").string());
        this->schedule_next_request(true);
        this->request_state_ = request_state::empty;

        if(config::license->isPremium()) {
            logMessage(LOG_INSTANCE, strobf("License has been validated.").string());
        } else {
            logMessage(LOG_INSTANCE, strobf("Instance integrity has been validated.").string());
        }

        if(!config::server::check_server_version_with_license())
            handle_check_fail(strobf("memory invalid").string());
    }

    {
        std::unique_lock slock{this->sync_request_lock};
        this->sync_request_cv.notify_all();
    }
}

void LicenseService::handle_check_fail(const std::string &error) {
    {
        std::lock_guard rlock{this->request_lock};
        this->abort_request(rlock, "request failed");

        //1 + 5*4 + 5*10+20*30
        //1 + 5*4 + 5*10+70*30
        auto soft_license_check = config::license->isValid() && (
                                            this->timings.last_succeeded.time_since_epoch().count() == 0 ? this->timings.failed_count < 32 : /* About 12hours */
                                            this->timings.failed_count < 82 /* about 36 hours */
        );
        const auto invalid_memory = !config::server::check_server_version_with_license();
        if(invalid_memory || (config::license->isPremium() && !soft_license_check)) {
            logCritical(LOG_INSTANCE, strobf("Failed to validate license:").string());
            logCritical(LOG_INSTANCE, invalid_memory ? strobf("invalid memory").string() : error);
            logCritical(LOG_INSTANCE, strobf("Stopping server!").string());
            ts::server::shutdownInstance();
        } else {
            logError(LOG_INSTANCE, strobf("Failed to validate instance integrity:").string());
            logError(LOG_INSTANCE, error);
        }

        this->schedule_next_request(false);
        this->request_state_ = request_state::empty;
    }

    {
        std::unique_lock slock{this->sync_request_lock};
        this->sync_request_cv.notify_all();
    }
}

void LicenseService::handle_dns_lookup_result(bool success, const std::variant<std::string, sockaddr_in> &result) {
    if(!success) {
        this->handle_check_fail(std::get<std::string>(result));
        return;
    }

    std::lock_guard rlock{this->request_lock};
    if(this->request_state_ != request_state::dns_lookup) {
        logError(LOG_INSTANCE, strobf("Request state isn't dns lookup anymore. Aborting dns lookup result callback.").string());
        return;
    }
    this->request_state_ = request_state::connecting;

    assert(!this->current_client);
    this->current_client = std::make_shared<::license::client::LicenseServerClient>(std::get<sockaddr_in>(result), 3);
    this->current_client->callback_connected = [&]{ this->handle_client_connected(); };
    this->current_client->callback_disconnected = [&](bool expected, const std::string& error) {
        this->handle_client_disconnected(error);
    };
    this->current_client->callback_message = [&](auto a, auto b, auto c) {
        this->handle_message(a, b, c);
    };

    std::string error{};
    if(!this->current_client->start_connection(error))
        this->handle_check_fail(strobf("connect failed: ").string() + error);
}

void LicenseService::client_send_message(::license::protocol::PacketType type, ::google::protobuf::Message &message) {
    auto buffer = message.SerializeAsString();

    assert(this->current_client);
    this->current_client->send_message(type, buffer.data(), buffer.length());
}

void LicenseService::handle_client_connected() {
    {
        if(this->verbose_)
            debugMessage(LOG_INSTANCE, strobf("License client connected").string());

        std::lock_guard rlock{this->request_lock};
        if(this->request_state_ != request_state::connecting) {
            logError(LOG_INSTANCE, strobf("Request state isn't connecting anymore. Aborting client connect callback.").string());
            return;
        }

        this->request_state_ = request_state::license_validate;
    }

    this->send_license_validate_request();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
void LicenseService::handle_message(::license::protocol::PacketType type, const void *buffer, size_t size) {
    switch (type) {
        case ::license::protocol::PACKET_SERVER_VALIDATION_RESPONSE:
            this->handle_message_license_info(buffer, size);
            return;

        case ::license::protocol::PACKET_SERVER_PROPERTY_ADJUSTMENT:
            this->handle_message_property_adjustment(buffer, size);
            return;

        case ::license::protocol::PACKET_SERVER_LICENSE_UPGRADE_RESPONSE:
            this->handle_message_license_update(buffer, size);
            return;

        default:
            this->handle_check_fail(strobf("received unknown packet").string());
            return;
    }
}
#pragma GCC diagnostic pop

void LicenseService::handle_client_disconnected(const std::string& message) {
    std::lock_guard rlock{this->request_lock};
    if(this->request_state_ != request_state::finishing) {
        this->handle_check_fail(strobf("unexpected disconnect: ").string() + message);
        return;
    }

    this->abort_request(rlock, "");
}

void LicenseService::send_license_validate_request() {
    this->license_request_data = serverInstance->generateLicenseData();

    ts::proto::license::ServerValidation request{};
    if(this->license_request_data->license) {
        request.set_licensed(true);
        request.set_license_info(true);
        request.set_license(exportLocalLicense(this->license_request_data->license));
    } else {
        request.set_licensed(false);
        request.set_license_info(false);
    }
    request.set_memory_valid(config::server::check_server_version_with_license());
    request.mutable_info()->set_uname(this->license_request_data->info.uname);
    request.mutable_info()->set_version(this->license_request_data->info.version);
    request.mutable_info()->set_timestamp(this->license_request_data->info.timestamp.count());
    request.mutable_info()->set_unique_id(this->license_request_data->info.unique_id);
    this->client_send_message(::license::protocol::PACKET_CLIENT_SERVER_VALIDATION, request);
}

void LicenseService::handle_message_license_info(const void *buffer, size_t buffer_length) {
    std::lock_guard rlock{this->request_lock};
    if(this->request_state_ != request_state::license_validate) {
        this->handle_check_fail(strobf("invalid request state for license response packet").string());
        return;
    }

    ts::proto::license::LicenseResponse response{};
    if(!response.ParseFromArray(buffer, buffer_length)) {
        this->handle_check_fail(strobf("failed to parse license response packet").string());
        return;
    }

    if(response.has_blacklist()) {
        auto blacklist_state = response.blacklist().state();
        if(blacklist_state == ::ts::proto::license::BLACKLISTED) {
            this->abort_request(rlock, strobf("blacklist action").string());

            logCritical(LOG_INSTANCE, strobf("This TeaSpeak-Server instance has been blacklisted by TeaSpeak.").string());
            logCritical(LOG_INSTANCE, strobf("Stopping server!").string());
            ts::server::shutdownInstance();
            return;
        }
    }

    if(!response.valid()) {
        std::string reason{};
        if(response.has_invalid_reason())
            reason = response.invalid_reason();
        else
            reason = strobf("no reason given").string();

        license_invalid_reason = reason;
    } else {
        license_invalid_reason.reset();
    }

    if(response.has_update_pending() && response.update_pending()) {
        if(this->send_license_update_request()) {
            this->request_state_ = request_state::license_upgrade;
            return;
        }
    }

    if(this->license_invalid_reason.has_value()) {
        this->handle_check_fail(strobf("Failed to verify license (").string() + *this->license_invalid_reason + ")");
        return;
    }

    if(response.has_deprecate_third_party_clients() && response.deprecate_third_party_clients()) {
        config::server::clients::extra_welcome_message_type_teamspeak = config::server::clients::WELCOME_MESSAGE_TYPE_CHAT;
        config::server::clients::extra_welcome_message_teamspeak = strobf("\n[b][color=red]Your client version is not supported![/color]\nPlease download the newest TeaSpeak - Client at [url=https://teaspeak.de/?ref=unsupported-client]https://teaspeak.de/[/url].[/b]\n\nNew client features:\n- Video and Screen sharing\n- Enchanted chat system (Cross channel, emojies, images etc)\n- Video watch2gather\n- And a lot more!").string();
    }

    this->send_property_update_request();
    this->request_state_ = request_state::property_update;
}

void LicenseService::send_property_update_request() {
    auto data = this->license_request_data;
    if(!data) {
        this->handle_check_fail(strobf("missing property data").string());
        return;
    }


    ts::proto::license::PropertyUpdateRequest infos{};
    infos.set_speach_total(this->license_request_data->metrics.speech_total);
    infos.set_speach_dead(this->license_request_data->metrics.speech_dead);
    infos.set_speach_online(this->license_request_data->metrics.speech_online);
    infos.set_speach_varianz(this->license_request_data->metrics.speech_varianz);

    infos.set_clients_online(this->license_request_data->metrics.client_online);
    infos.set_bots_online(this->license_request_data->metrics.bots_online);
    infos.set_queries_online(this->license_request_data->metrics.queries_online);
    infos.set_servers_online(this->license_request_data->metrics.servers_online);
    infos.set_web_clients_online(this->license_request_data->metrics.web_clients_online);

    infos.set_web_cert_revision(this->license_request_data->web_certificate_revision);

    this->client_send_message(::license::protocol::PACKET_CLIENT_PROPERTY_ADJUSTMENT, infos);
}

void LicenseService::handle_message_property_adjustment(const void *buffer, size_t buffer_length) {
    std::lock_guard rlock{this->request_lock};
    if(this->request_state_ != request_state::property_update) {
        this->handle_check_fail(strobf("invalid request state for property update packet").string());
        return;
    }

    ts::proto::license::PropertyUpdateResponse response{};
    if(!response.ParseFromArray(buffer, buffer_length)) {
        this->handle_check_fail(strobf("failed to parse property update packet").string());
        return;
    }

    if(response.has_web_certificate()) {
        auto& certificate = response.web_certificate();
        serverInstance->setWebCertRoot(certificate.key(), certificate.certificate(), certificate.revision());
    }

    if(response.has_reset_speach())
        serverInstance->resetSpeechTime();
    serverInstance->properties()[property::SERVERINSTANCE_SPOKEN_TIME_VARIANZ] = response.speach_varianz_corrector();

    this->request_state_ = request_state::finishing;
    this->handle_check_succeeded();
}

bool LicenseService::send_license_update_request() {
    ts::proto::license::RequestLicenseUpgrade request{};
    this->client_send_message(::license::protocol::PACKET_CLIENT_LICENSE_UPGRADE, request);
    return true;
}

inline std::string format_time(const std::chrono::system_clock::time_point& time);
void LicenseService::handle_message_license_update(const void *buffer, size_t buffer_length) {
    std::lock_guard rlock{this->request_lock};
    if(this->request_state_ != request_state::license_upgrade) {
        this->handle_check_fail(strobf("invalid request state for license upgrade packet").string());
        return;
    }

    ts::proto::license::LicenseUpgradeResponse response{};
    if(!response.ParseFromArray(buffer, buffer_length)) {
        this->handle_check_fail(strobf("failed to parse license upgrade packet").string());
        return;
    }

    if(!response.valid()) {
        logError(LOG_INSTANCE, strobf("Failed to upgrade license: {}").string(), response.error_message());
        goto error_exit;
    } else {
        std::string error{};
        auto license_data = response.license_key();
        auto license = ::license::readLocalLicence(license_data, error);
        if(!license) {
            logError(LOG_INSTANCE, strobf("Failed to parse received upgraded license key: {}").string(), error);
            goto error_exit;
        }
        if(!license->isValid()) {
            logError(LOG_INSTANCE, strobf("Received license seems to be invalid.").string());
            goto error_exit;
        }

        auto end = std::chrono::system_clock::time_point{} + std::chrono::milliseconds{license->data.endTimestamp};
        logMessage(LOG_INSTANCE, strobf("Received new license registered to {}, valid until {}").string(), license->data.licenceOwner, format_time(end));
        if(!config::update_license(error, license_data))
            logError(LOG_INSTANCE, strobf("Failed to write new license key to config file: {}").string(), error);

        config::license = license;

        this->send_license_validate_request();
        this->request_state_ = request_state::license_validate;
    }

    return;
    error_exit:
    logError(LOG_INSTANCE, strobf("License upgrade failed. Using old key.").string());
    if(this->license_invalid_reason.has_value()) {
        this->handle_check_fail(strobf("Failed to verify license (").string() + *this->license_invalid_reason + ")");
        return;
    }

    this->send_property_update_request();
    this->request_state_ = request_state::property_update;
}

/* request scheduler */
inline std::string format_time(const std::chrono::system_clock::time_point& time) {
    std::time_t now = std::chrono::system_clock::to_time_t(time);
    std::tm * ptm = std::localtime(&now);
    char buffer[128];
    const auto length = std::strftime(buffer, 128, "%a, %d.%m.%Y %H:%M:%S", ptm);
    return std::string{buffer, length};
}

void LicenseService::schedule_next_request(bool request_success) {
    auto& fail_count = this->timings.failed_count;
    if(request_success)
        fail_count = 0;
    else
        fail_count++;

    std::chrono::milliseconds next_request;
    if(fail_count == 0)
        next_request = std::chrono::hours{2};
    else if(fail_count <= 1)
        next_request = std::chrono::minutes(1);
    else if(fail_count <= 5)
        next_request = std::chrono::minutes(5);
    else if(fail_count <= 10)
        next_request = std::chrono::minutes(10);
    else
        next_request = std::chrono::minutes(30);
#ifdef DO_LOCAL_REQUEST
    next_request = std::chrono::seconds(30);
#endif

    this->timings.next_request = this->timings.last_request + next_request;
    if(this->verbose_) {
        logMessage(LOG_INSTANCE, strobf("Scheduling next check at {}").c_str(), format_time(this->timings.next_request));
    }
}

void LicenseService::execute_tick() {
    std::unique_lock rlock{this->request_lock, std::try_to_lock}; /* It will be slightly blocking when its within the message hendling */
    if(!rlock) return;

    /* do it not above because if we might have a deadlock here we don't want to punish the user */
    if(this->timings.last_succeeded.time_since_epoch().count() == 0) {
        auto difference = config::license->isPremium() ? std::chrono::hours{24 * 4} : std::chrono::hours{24 * 7};
        if(std::chrono::steady_clock::now() - difference > this->startup_timepoint_) {
            this->startup_timepoint_ = std::chrono::steady_clock::now(); /* shut down only once */

            if(config::license->isPremium()) {
                logCritical(LOG_INSTANCE, strobf("Failed to validate license within 4 days.").string());
            } else {
                logCritical(LOG_INSTANCE, strobf("Failed to validate instance integrity within 7 days.").string());
            }
            logCritical(LOG_INSTANCE, strobf("Stopping server!").string());
            ts::server::shutdownInstance();
            return;
        }
    }

    auto now = std::chrono::system_clock::now();
    if(this->request_state_ != request_state::empty) {
        if(this->timings.last_request + std::chrono::minutes{5} < now) {
            this->handle_check_fail(strobf("check timeout").string());
        } else {
            return;
        }
    }

    if(std::chrono::system_clock::now() > this->timings.next_request) {
        this->begin_request();
    }
}