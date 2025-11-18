//
// Created by WolverinDEV on 24/10/2020.
//

#include "./lib.h"
#include "./imports.h"
#include "../client/SpeakingClient.h"
#include "../client/voice/VoiceClient.h"
#include <Definitions.h>
#include <log/LogUtils.h>

using namespace ts;
using namespace ts::server;
using namespace ts::rtc;

struct LibCallbackData {
    ClientId client_id;
    std::weak_ptr<SpeakingClient> weak_ref;
};

#define log(...)                    \
switch(level) {                     \
    case 0:                         \
        logTrace(__VA_ARGS__);      \
        break;                      \
    case 1:                         \
        debugMessage(__VA_ARGS__);  \
        break;                      \
    case 2:                         \
        logMessage(__VA_ARGS__);    \
        break;                      \
    case 3:                         \
        logWarning(__VA_ARGS__);    \
        break;                      \
    case 4:                         \
        logError(__VA_ARGS__);      \
        break;                      \
    case 5:                         \
        logCritical(__VA_ARGS__);   \
        break;                      \
    default:                        \
        debugMessage(__VA_ARGS__);  \
        break;                      \
}

void librtc_callback_log(uint8_t level, const void* callback_data_ptr, const char* message_ptr, uint32_t length) {
    auto callback_data = (LibCallbackData*) callback_data_ptr;
    std::string_view message{message_ptr, length};

    if(callback_data) {
        auto source_client = callback_data->weak_ref.lock();
        if(!source_client) { return; }

        log(source_client->getServerId(), "{} [WebRTC] {}", CLIENT_STR_LOG_PREFIX_(source_client), message);
    } else {
        log(LOG_GENERAL, "{}", message);
    }
}

#undef log

void librtc_callback_free_client_data(const void* data) {
    delete (LibCallbackData*) data;
}

uint32_t librtc_callback_rtc_configure(const void* callback_data_ptr, void* configure_ptr) {
    auto callback_data = (LibCallbackData*) callback_data_ptr;

    auto target_client = callback_data->weak_ref.lock();
    if(!target_client) { return 2; }

    RtpClientConfigureOptions options{};
    memset(&options, 0, sizeof(options));

    options.ice_tcp = config::web::tcp_enabled;
    options.ice_udp = config::web::udp_enabled;
    options.ice_upnp = config::web::enable_upnp;
    options.min_port = config::web::webrtc_port_min;
    options.max_port = config::web::webrtc_port_max;

    std::string stun_host{config::web::stun_host};
    if(!net::is_ipv4(stun_host) && !net::is_ipv6(stun_host)) {
        auto timestamp_begin = std::chrono::system_clock::now();
        auto result = gethostbyname(stun_host.c_str());
        auto timestamp_end = std::chrono::system_clock::now();

        if(!result) {
            logError(target_client->getServerId(), "{} Failed to resolve stun hostname {}: {}", target_client->getLoggingPrefix(), stun_host, strerror(errno));
            return 1;
        }

        auto addresses = (struct in_addr **) result->h_addr_list;
        if(!*addresses) {
            logError(target_client->getServerId(), "{} Failed to resolve stun hostname {}: Empty result", target_client->getLoggingPrefix(), stun_host);
            return 1;
        }

        stun_host = inet_ntoa(**addresses);
        logTrace(target_client->getServerId(), "{} Resolved stun host {} to {} within {}ms", target_client->getLoggingPrefix(), config::web::stun_host, stun_host,
            std::chrono::duration_cast<std::chrono::milliseconds>(timestamp_end - timestamp_begin).count()
        );
    }
    options.stun_host = stun_host.c_str();
    options.stun_port = config::web::stun_port;
    if(!config::web::stun_enabled) {
        options.stun_port = 0;
        options.stun_host = nullptr;
    }

    auto result = librtc_rtc_configure(configure_ptr, &options, sizeof(options));
    if(result) {
        logError(target_client->getServerId(), "{} Failed to configure RTC connection: {}", CLIENT_STR_LOG_PREFIX_(target_client), result);
        librtc_free_str(result);
        return 1;
    }
    return 0;
}

void librtc_callback_client_stream_assignment(const void* callback_data_ptr, uint32_t stream_id, uint8_t media_type, const void* source_data_ptr) {
    auto callback_data = (LibCallbackData*) callback_data_ptr;
    auto source_data = (LibCallbackData*) source_data_ptr;

    auto target_client = callback_data->weak_ref.lock();
    if(!target_client) { return; }

    if(source_data) {
        auto source_client = source_data->weak_ref.lock();
        if(!source_client) { return; }

        ts::command_builder notify{"notifyrtcstreamassignment"};
        notify.put_unchecked(0, "streamid", stream_id);
        notify.put_unchecked(0, "media", media_type);
        notify.put_unchecked(0, "sclid", source_client->getClientId());
        notify.put_unchecked(0, "scluid", source_client->getUid());
        notify.put_unchecked(0, "scldbid", source_client->getClientDatabaseId());
        notify.put_unchecked(0, "sclname", source_client->getDisplayName());
        target_client->sendCommand(notify);
    } else {
        ts::command_builder notify{"notifyrtcstreamassignment"};
        notify.put_unchecked(0, "streamid", stream_id);
        notify.put_unchecked(0, "sclid", 0);
        target_client->sendCommand(notify);
    }
}

void librtc_callback_client_offer_generated(const void* callback_data_ptr, const char* offer, size_t offer_length) {
    auto callback_data = (LibCallbackData*) callback_data_ptr;
    auto target_client = callback_data->weak_ref.lock();
    if(!target_client) { return; }

    ts::command_builder notify{"notifyrtcsessiondescription"};
    notify.put_unchecked(0, "mode", "offer");
    notify.put_unchecked(0, "sdp", std::string_view{offer, offer_length});
    target_client->sendCommand(notify);
    /* don't blame the client if we require him to do anything */
    target_client->rtc_session_pending_describe = true;
}

void librtc_callback_client_ice_candidate(const void* callback_data_ptr, uint32_t media_line, const char* candidate, size_t candidate_length) {
    auto callback_data = (LibCallbackData*) callback_data_ptr;
    auto target_client = callback_data->weak_ref.lock();
    if(!target_client) { return; }

    ts::command_builder notify{"notifyrtcicecandidate"};
    notify.put_unchecked(0, "medialine", media_line);
    notify.put_unchecked(0, "candidate", std::string_view{candidate, candidate_length});
    target_client->sendCommand(notify);
}

void librtc_callback_client_stream_start(const void* callback_data_ptr, uint32_t stream_id, const void* source_data_ptr) {
    auto callback_data = (LibCallbackData*) callback_data_ptr;
    auto source_data = (LibCallbackData*) source_data_ptr;

    auto target_client = callback_data->weak_ref.lock();
    if(!target_client) { return; }

    auto source_client = source_data->weak_ref.lock();
    if(!source_client) { return; }

    ts::command_builder notify{"notifyrtcstreamstate"};
    notify.put_unchecked(0, "streamid", stream_id);
    notify.put_unchecked(0, "sclid", source_client->getClientId());
    notify.put_unchecked(0, "scluid", source_client->getUid());
    notify.put_unchecked(0, "scldbid", source_client->getClientDatabaseId());
    notify.put_unchecked(0, "sclname", source_client->getDisplayName());
    notify.put_unchecked(0, "state", "1");
    target_client->sendCommand(notify);
}

void librtc_callback_client_video_broadcast_info(const void* const* callback_data_array, uint32_t callback_data_length, const BroadcastInfo* broadcasts, uint32_t broadcast_count) {
    ts::command_builder notify{"notifybroadcastvideo", 32, broadcast_count};

    size_t bulk_index{0};
    for(size_t index{0}; index < broadcast_count; index++) {
        auto& broadcast = broadcasts[index];
        auto source_data = (LibCallbackData*) broadcast.broadcasting_client_data;
        auto source_client = source_data->weak_ref.lock();
        if(!source_client) { continue; }

        auto bulk = notify.bulk(bulk_index++);
        bulk.put_unchecked("bt", broadcast.broadcast_type);
        bulk.put_unchecked("bid", broadcast.broadcasting_client_id);
        bulk.put_unchecked("sclid", source_client->getClientId());
    }

    for(size_t index{0}; index < callback_data_length; index++) {
        auto client_data = (LibCallbackData*) callback_data_array[index];
        auto client = client_data->weak_ref.lock();
        if(!client) { continue; }

        client->sendCommand(notify);
        if(client->getType() == ClientType::CLIENT_TEAMSPEAK) {
            auto voice_client = std::dynamic_pointer_cast<VoiceClient>(client);
            assert(voice_client);

            if(broadcast_count > 0) {
                voice_client->send_video_unsupported_message();
            } else {
                voice_client->clear_video_unsupported_message_flag();
            }
        }
    }
}

void librtc_callback_client_stream_stop(const void* callback_data_ptr, uint32_t stream_id, const void* source_data_ptr) {
    auto callback_data = (LibCallbackData*) callback_data_ptr;
    auto source_data = (LibCallbackData*) source_data_ptr;

    auto target_client = callback_data->weak_ref.lock();
    if(!target_client) { return; }

    auto source_client = source_data->weak_ref.lock();
    if(!source_client) { return; }

    ts::command_builder notify{"notifyrtcstreamstate"};
    notify.put_unchecked(0, "streamid", stream_id);
    notify.put_unchecked(0, "sclid", source_client->getClientId());
    notify.put_unchecked(0, "state", "0");
    target_client->sendCommand(notify);
}

void librtc_callback_client_audio_sender_data(const void* callback_data_ptr, const void* source_data_ptr, uint8_t mode, uint16_t seq_no, uint8_t codec, const void* data, uint32_t length) {
    auto callback_data = (LibCallbackData*) callback_data_ptr;
    auto source_data = (LibCallbackData*) source_data_ptr;

    /* Target client must be a voice client. The web client does not uses the native audio client */
    auto target_client = std::dynamic_pointer_cast<VoiceClient>(callback_data->weak_ref.lock());
    if(!target_client) { return; }

    auto source_client = source_data->weak_ref.lock();
    if(!source_client) { return; }

    if(mode == 0) {
        if(!target_client->shouldReceiveVoice(source_client)) {
            return;
        }

        target_client->send_voice(source_client, seq_no, codec, data, length);
    } else if(mode == 1) {
        if(!target_client->shouldReceiveVoiceWhisper(source_client)) {
            return;
        }

        target_client->send_voice_whisper(source_client, seq_no, codec, data, length);
    } else {
        /* we've received audio with an invalid mode.... */
    }
}

void librtc_client_whisper_session_reset(const void* callback_data_ptr) {
    auto callback_data = (LibCallbackData*) callback_data_ptr;

    auto client = std::dynamic_pointer_cast<SpeakingClient>(callback_data->weak_ref.lock());
    if(!client) { return; }

    client->whisper_handler().handle_session_reset();

    ts::command_builder notify{"notifywhispersessionreset"};
    client->sendCommand(notify);
}

void librtc_callback_client_video_join(const void* callback_data_ptr, uint32_t stream_id, const void* target_data_ptr) {
    auto callback_data = (LibCallbackData*) callback_data_ptr;
    auto target_data = (LibCallbackData*) target_data_ptr;

    auto stream_client = callback_data->weak_ref.lock();
    if(!stream_client) { return; }

    auto target_client = target_data->weak_ref.lock();
    if(!target_client) { return; }

    ts::command_builder notify{"notifystreamjoined"};
    notify.put_unchecked(0, "streamid", stream_id);
    notify.put_unchecked(0, "clid", target_client->getClientId());
    notify.put_unchecked(0, "cluid", target_client->getUid());
    notify.put_unchecked(0, "cldbid", target_client->getClientDatabaseId());
    notify.put_unchecked(0, "clname", target_client->getDisplayName());
    stream_client->sendCommand(notify);
}

void librtc_callback_client_video_leave(const void* callback_data_ptr, uint32_t stream_id, const void* target_data_ptr) {
    auto callback_data = (LibCallbackData*) callback_data_ptr;
    auto target_data = (LibCallbackData*) target_data_ptr;

    auto stream_client = callback_data->weak_ref.lock();
    if(!stream_client) { return; }

    auto target_client = target_data->weak_ref.lock();
    if(!target_client) { return; }

    ts::command_builder notify{"notifystreamleft"};
    notify.put_unchecked(0, "streamid", stream_id);
    notify.put_unchecked(0, "clid", target_client->getClientId());
    stream_client->sendCommand(notify);
}

static NativeCallbacks native_callbacks{
        .version = 7,

        .log = librtc_callback_log,
        .free_client_data = librtc_callback_free_client_data,

        .rtc_configure = librtc_callback_rtc_configure,

        .client_stream_assignment = librtc_callback_client_stream_assignment,
        .client_offer_generated = librtc_callback_client_offer_generated,
        .client_ice_candidate = librtc_callback_client_ice_candidate,

        .client_stream_start = librtc_callback_client_stream_start,
        .client_stream_stop = librtc_callback_client_stream_stop,

        .client_video_join = librtc_callback_client_video_join,
        .client_video_leave = librtc_callback_client_video_leave,

        .client_video_broadcast_info = librtc_callback_client_video_broadcast_info,

        .client_audio_sender_data = librtc_callback_client_audio_sender_data,
        .client_whisper_session_reset = librtc_client_whisper_session_reset
};

std::string_view rtc::version() {
    return std::string_view{librtc_version()};
}

bool rtc::initialize(std::string &error) {
    auto error_ptr = librtc_init(&native_callbacks, sizeof native_callbacks);
    if(!error_ptr) { return true; }

    error = std::string{error_ptr};
    librtc_free_str(error_ptr);
    return false;
}

Server::Server() {
    this->server_ptr = librtc_create_server();
}

Server::~Server() {
    librtc_destroy_server(this->server_ptr);
}

RTCClientId Server::create_client(const std::shared_ptr<server::SpeakingClient> &client) {
    auto data = new LibCallbackData{
            .client_id = client->getClientId(),
            .weak_ref = client
    };

    return librtc_create_client(this->server_ptr, data);
}

bool Server::initialize_rtc_connection(std::string &error, RTCClientId client_id) {
    auto error_ptr = librtc_initialize_rtc_connection(this->server_ptr, client_id);
    if(!error_ptr) { return true; }

    error = std::string{error_ptr};
    librtc_free_str(error_ptr);
    return false;
}

bool Server::initialize_native_connection(std::string &error, RTCClientId client_id) {
    auto error_ptr = librtc_initialize_native_connection(this->server_ptr, client_id);
    if(!error_ptr) { return true; }

    error = std::string{error_ptr};
    librtc_free_str(error_ptr);
    return false;
}

void Server::destroy_client(RTCClientId client_id) {
    librtc_destroy_client(this->server_ptr, client_id);
}

bool Server::client_video_stream_count(uint32_t client_id, uint32_t *camera, uint32_t *desktop) {
    return librtc_client_video_stream_count(this->server_ptr, client_id, camera, desktop) == 0;
}

VideoBroadcastConfigureResult Server::client_broadcast_video_configure(RTCClientId client_id, VideoBroadcastType broadcast_type,
                                                                       const VideoBroadcastOptions *options) {
    auto result = librtc_client_broadcast_video_configure(this->server_ptr, client_id, (uint8_t) broadcast_type, options);
    switch(result) {
        case 0x00: return VideoBroadcastConfigureResult::Success;
        case 0x01: return VideoBroadcastConfigureResult::InvalidBroadcastType;
        case 0x02: return VideoBroadcastConfigureResult::InvalidClient;
        case 0x03: return VideoBroadcastConfigureResult::InvalidBroadcast;
        default: return VideoBroadcastConfigureResult::UnknownError;
    }
}

VideoBroadcastConfigureResult Server::client_broadcast_video_config(RTCClientId client_id, VideoBroadcastType broadcast_type,
                                                                    VideoBroadcastOptions *options) {
    auto result = librtc_client_broadcast_video_config(this->server_ptr, client_id, (uint8_t) broadcast_type, options);
    switch(result) {
        case 0x00: return VideoBroadcastConfigureResult::Success;
        case 0x01: return VideoBroadcastConfigureResult::InvalidBroadcastType;
        case 0x02: return VideoBroadcastConfigureResult::InvalidClient;
        case 0x03: return VideoBroadcastConfigureResult::InvalidBroadcast;
        default: return VideoBroadcastConfigureResult::UnknownError;
    }
}

void Server::reset_rtp_session(RTCClientId client_id) {
    librtc_reset_rtp_session(this->server_ptr, client_id);
}

bool Server::apply_remote_description(std::string &error, RTCClientId client_id, uint32_t mode, const std::string &description) {
    auto error_ptr = librtc_apply_remote_description(this->server_ptr, client_id, mode, description.c_str());
    if(!error_ptr) { return true; }

    error = std::string{error_ptr};
    librtc_free_str(error_ptr);
    return false;
}

bool Server::generate_local_description(RTCClientId client, std::string &result) {
    char* description_ptr;
    auto error_ptr = librtc_generate_local_description(this->server_ptr, client, &description_ptr);
    if(error_ptr) {
        result = std::string{error_ptr};
        librtc_free_str(error_ptr);
        return false;
    } else {
        result = std::string{description_ptr};
        librtc_free_str(description_ptr);
        return true;
    }
}

bool Server::add_ice_candidate(std::string &error, RTCClientId client_id, uint32_t media_line, const std::string &description) {
    auto error_ptr = librtc_add_ice_candidate(this->server_ptr, client_id, media_line, description.length() == 0 ? nullptr : description.c_str());
    if(!error_ptr) { return true; }

    error = std::string{error_ptr};
    librtc_free_str(error_ptr);
    return false;
}

void Server::ice_candidates_finished(RTCClientId) {
    /* Nothing really to do here */
}

uint32_t Server::create_channel() {
    return librtc_create_channel(this->server_ptr);
}

ChannelAssignResult Server::assign_channel(RTCClientId client_id, RTCChannelId channel_id) {
    auto result = librtc_assign_channel(this->server_ptr, client_id, channel_id);
    switch(result) {
        case 0x00: return ChannelAssignResult::Success;
        case 0x01: return ChannelAssignResult::ClientUnknown;
        case 0x02: return ChannelAssignResult::TargetChannelUnknown;
        default: return ChannelAssignResult::UnknownError;
    }
}

BroadcastStartResult Server::start_broadcast_audio(RTCClientId client_id, RTCStreamId track_id) {
    auto result = librtc_client_broadcast_audio(this->server_ptr, client_id, track_id);
    switch(result) {
        case 0x00: return BroadcastStartResult::Success;
        case 0x01: return BroadcastStartResult::InvalidClient;
        case 0x02: return BroadcastStartResult::ClientHasNoChannel;
        case 0x03: return BroadcastStartResult::InvalidBroadcastType;
        case 0x04: return BroadcastStartResult::InvalidStreamId;
        default:
            logCritical(LOG_GENERAL, "Audio broadcast start returned unknown error code: {}", result);
            return BroadcastStartResult::UnknownError;
    }
}

BroadcastStartResult Server::start_broadcast_video(RTCClientId client_id, VideoBroadcastType broadcast_type, RTCStreamId track_id, const VideoBroadcastOptions* options) {
    auto result = librtc_client_broadcast_video(this->server_ptr, client_id, (uint8_t) broadcast_type, track_id, options);
    switch(result) {
        case 0x00: return BroadcastStartResult::Success;
        case 0x01: return BroadcastStartResult::InvalidClient;
        case 0x02: return BroadcastStartResult::ClientHasNoChannel;
        case 0x03: return BroadcastStartResult::InvalidBroadcastType;
        case 0x04: return BroadcastStartResult::InvalidStreamId;
        default:
            logCritical(LOG_GENERAL, "Video broadcast start returned unknown error code: {}", result);
            return BroadcastStartResult::UnknownError;
    }
}

VideoBroadcastJoinResult Server::join_video_broadcast(RTCClientId client_id, RTCClientId target_client_id, VideoBroadcastType video_broadcast_type) {
    auto result = librtc_video_broadcast_join(this->server_ptr, client_id, target_client_id, (uint8_t) video_broadcast_type);
    switch(result) {
        case 0x00: return VideoBroadcastJoinResult::Success;
        case 0x01: return VideoBroadcastJoinResult::InvalidBroadcastType;
        case 0x02: return VideoBroadcastJoinResult::InvalidClient;
        case 0x03: return VideoBroadcastJoinResult::InvalidBroadcast;
        default:
            logCritical(LOG_GENERAL, "Video broadcast join returned unknown error code: {}", result);
            return VideoBroadcastJoinResult::UnknownError;
    }
}

void Server::leave_video_broadcast(RTCClientId client_id, RTCClientId target_client_id, VideoBroadcastType video_broadcast_type) {
    librtc_video_broadcast_leave(this->server_ptr, client_id, target_client_id, (uint8_t) video_broadcast_type);
}

std::optional<NativeAudioSourceSupplier> Server::create_audio_source_supplier_sender(uint32_t client_id, uint32_t stream_id) {
    auto result = librtc_create_audio_source_supplier(this->server_ptr, client_id, stream_id);
    if(!result) { return std::nullopt; }

    return std::make_optional<NativeAudioSourceSupplier>(result);
}

void Server::destroy_channel(uint32_t channel_id) {
    librtc_destroy_channel(this->server_ptr, channel_id);
}

bool Server::configure_whisper_session(std::string &error, RTCClientId client_id, uint32_t source_stream_id, RTCClientId *client_ids, size_t client_id_count) {
    auto error_ptr = librtc_whisper_configure(this->server_ptr, client_id, source_stream_id, client_ids, client_id_count);
    if(!error_ptr) { return true; }

    error = std::string{error_ptr};
    librtc_free_str(error_ptr);
    return false;
}

void Server::reset_whisper_session(RTCClientId client_id) {
    librtc_whisper_reset(this->server_ptr, client_id);
}

NativeAudioSourceSupplier::NativeAudioSourceSupplier(void *ptr) : sender_ptr{ptr} {}
NativeAudioSourceSupplier::NativeAudioSourceSupplier(NativeAudioSourceSupplier &&other) noexcept : sender_ptr{other.sender_ptr} {
    other.sender_ptr = nullptr;
}
NativeAudioSourceSupplier::~NativeAudioSourceSupplier() noexcept {
    if(this->sender_ptr) {
        librtc_destroy_audio_source_supplier(this->sender_ptr);
    }
}

void NativeAudioSourceSupplier::send_audio(uint16_t seq_no, bool marked, uint32_t timestamp, uint8_t codec, const std::string_view &data) {
    if(this->sender_ptr) {
        librtc_audio_source_supply(this->sender_ptr, seq_no, marked, timestamp, codec, data.empty() ? nullptr : data.data(), data.length());
    }
}