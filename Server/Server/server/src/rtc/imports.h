#include <cstdint>
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

struct BroadcastInfo {
    uint32_t broadcasting_client_id;
    const void* broadcasting_client_data;
    uint8_t broadcast_type;
};

/* Attention: Do not call any librtc functions within being in the callback, only librtc_destroy_client is allowed */
struct NativeCallbacks {
    uint32_t version;

    void(*log)(uint8_t /* level */, const void* /* callback data */, const char* /* message */, uint32_t /* length */);
    void(*free_client_data)(const void*);

    uint32_t(*rtc_configure)(const void* /* callback data */, void* /* configure callback data */);

    void(*client_stream_assignment)(const void* /* callback data */, uint32_t /* stream id */, uint8_t /* media type */, const void* /* source callback data */);
    void(*client_offer_generated)(const void* /* callback data */, const char* /* offer */, size_t /* offer length */);
    void(*client_ice_candidate)(const void* /* callback data */, uint32_t /* media line */, const char* /* candidate */, size_t /* candidate length */);

    void(*client_stream_start)(const void* /* callback data */, uint32_t /* stream id */, const void* /* source callback data */);
    void(*client_stream_stop)(const void* /* callback data */, uint32_t /* stream id */, const void* /* source callback data */);

    void(*client_video_join)(const void* /* callback data */, uint32_t /* stream id */, const void* /* source callback data */);
    void(*client_video_leave)(const void* /* callback data */, uint32_t /* stream id */, const void* /* source callback data */);

    void(*client_video_broadcast_info)(const void* const* /* callback data array */, uint32_t /* callback data length */, const BroadcastInfo* /* broadcasts */, uint32_t /* broadcast count */);
    void(*client_audio_sender_data)(const void* /* callback data */, const void* /* source callback data */, uint8_t /* mode */, uint16_t /* seq. no. */, uint8_t /* codec */, const void* /* data */, uint32_t /* length */);

    void(*client_whisper_session_reset)(const void* /* callback data */);
};

struct RtpClientConfigureOptions {
    uint16_t min_port;
    uint16_t max_port;

    bool ice_tcp;
    bool ice_udp;
    bool ice_upnp;

    const char* stun_host;
    uint16_t stun_port;
};

struct VideoBroadcastOptions {
    constexpr static auto kOptionBitrate{0x01};
    constexpr static auto kOptionKeyframeInterval{0x02};

    uint32_t update_mask;
    uint32_t bitrate;
    uint32_t keyframe_interval;
};

extern const char* librtc_version();
extern void librtc_free_str(const char* /* ptr */);

extern const char* librtc_init(const NativeCallbacks* /* */, size_t /* size of the callback struct */);
extern const char* librtc_rtc_configure(void* /* callback data */, const RtpClientConfigureOptions* /* config */, size_t /* config size */);

extern void* librtc_create_server();
extern void librtc_destroy_server(void* /* server */);

extern uint32_t librtc_create_client(void* /* server */, void* /* callback data */);
extern void librtc_destroy_client(void* /* server */, uint32_t /* client id */);

extern const char* librtc_initialize_rtc_connection(void* /* server */, uint32_t /* client id */);
extern const char* librtc_initialize_native_connection(void* /* server */, uint32_t /* client id */);

extern const char* librtc_reset_rtp_session(void* /* server */, uint32_t /* client id */);
extern const char* librtc_apply_remote_description(void* /* server */, uint32_t /* client id */, uint32_t /* mode */, const char* /* description */);
extern const char* librtc_generate_local_description(void* /* server */, uint32_t /* client id */, char** /* description */);
extern const char* librtc_add_ice_candidate(void* /* server */, uint32_t /* client id */, uint32_t /* media line */, const char* /* candidate */);

extern uint32_t librtc_client_video_stream_count(void* /* server */, uint32_t /* client id */, uint32_t* /* camera count */, uint32_t* /* screen count */);

extern void* librtc_create_audio_source_supplier(void* /* server */, uint32_t /* client id */, uint32_t /* stream id */);
extern void librtc_audio_source_supply(void* /* sender */,
                                       uint16_t /* seq no */,
                                       bool /* marked */,
                                       uint32_t /* timestamp */,
                                       uint8_t /* codec */,
                                       const void* /* data */,
                                       uint32_t /* length */);
extern void librtc_destroy_audio_source_supplier(void* /* sender */);

extern uint32_t librtc_create_channel(void* /* server */);
extern uint32_t librtc_assign_channel(void* /* server */, uint32_t /* client id */, uint32_t /* channel id */);
extern void librtc_destroy_channel(void* /* server */, uint32_t /* channel */);

/* Audio functions */
extern uint32_t librtc_client_broadcast_audio(void* /* server */, uint32_t /* client id */, uint32_t /* stream id */);
extern const char* librtc_whisper_configure(void* /* server */, uint32_t /* client id */, uint32_t /* source stream id */, uint32_t* /* client ids */, uint32_t /* client id count */);
extern void librtc_whisper_reset(void* /* server */, uint32_t /* client id */);

/* Video functions */
extern uint32_t librtc_client_broadcast_video(void* /* server */, uint32_t /* client id */, uint8_t /* broadcast type */, uint32_t /* stream id */, const VideoBroadcastOptions* /* options */);
extern uint32_t librtc_client_broadcast_video_configure(void* /* callback data */, uint32_t /* client id */, uint8_t /* broadcast type */, const VideoBroadcastOptions* /* options */);
extern uint32_t librtc_client_broadcast_video_config(void* /* callback data */, uint32_t /* client id */, uint8_t /* broadcast type */, VideoBroadcastOptions* /* options */);

extern uint32_t librtc_video_broadcast_join(void* /* server */, uint32_t /* client id */, uint32_t /* target client id */, uint8_t /* broadcast type */);
extern void librtc_video_broadcast_leave(void* /* server */, uint32_t /* client id */, uint32_t /* target client id */, uint8_t /* broadcast type */);

#ifdef __cplusplus
};
#endif