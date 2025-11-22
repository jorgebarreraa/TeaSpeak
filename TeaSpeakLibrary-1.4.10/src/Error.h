#pragma once

#include <utility>
#include <memory>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <map>

namespace ts {
    namespace permission {
        enum PermissionType : uint16_t;
    }

    struct error {
        enum type : uint16_t {
            ok = 0x0,
            undefined = 0x1,
            not_implemented = 0x2,
            lib_time_limit_reached = 0x5,

            command_not_found = 0x100,
            unable_to_bind_network_port = 0x101,
            no_network_port_available = 0x102,

            /* Mainly used by the TeaClient */
            command_timed_out = 0x110,
            command_aborted_connection_closed = 0x111,

            client_invalid_id = 0x200,
            client_nickname_inuse = 0x201,
            invalid_error_code = 0x202,

            client_protocol_limit_reached = 0x203,
            client_invalid_type = 0x204,
            client_already_subscribed = 0x205,
            client_not_logged_in = 0x206,
            client_could_not_validate_identity = 0x207,
            client_invalid_password = 0x208,
            client_too_many_clones_connected = 0x209,
            client_version_outdated = 0x20a,
            client_is_online = 0x20b,
            client_is_flooding = 0x20c,
            client_hacked = 0x20d,
            client_cannot_verify_now = 0x20e,
            client_login_not_permitted = 0x20f,
            client_not_subscribed = 0x210,
            client_unknown = 0x0211,
            client_join_rate_limit_reached = 0x0212,
            client_is_already_member_of_group = 0x0213,
            client_is_not_member_of_group = 0x0214,
            client_type_is_not_allowed = 0x0215,

            channel_invalid_id = 0x300,
            channel_protocol_limit_reached = 0x301,
            channel_already_in = 0x302,
            channel_name_inuse = 0x303,
            channel_not_empty = 0x304,
            channel_can_not_delete_default = 0x305,
            channel_default_require_permanent = 0x306,
            channel_invalid_flags = 0x307,
            channel_parent_not_permanent = 0x308,
            channel_maxclients_reached = 0x309,
            channel_maxfamily_reached = 0x30a,
            channel_invalid_order = 0x30b,
            channel_no_filetransfer_supported = 0x30c,
            channel_invalid_password = 0x30d,
            channel_is_private_channel = 0x30e,
            channel_invalid_security_hash = 0x30f,
            channel_is_deleted = 0x310,
            channel_name_invalid = 0x311,
            channel_limit_reached = 0x312,
            
            channel_family_not_visible = 0x320,
            channel_default_require_visible = 0x321,

            server_invalid_id = 0x400,
            server_running = 0x401,
            server_is_shutting_down = 0x402,
            server_maxclients_reached = 0x403,
            server_invalid_password = 0x404,
            server_deployment_active = 0x405,
            server_unable_to_stop_own_server = 0x406,
            server_is_virtual = 0x407,
            server_wrong_machineid = 0x408,
            server_is_not_running = 0x409,
            server_is_booting = 0x40a,
            server_status_invalid = 0x40b,
            server_modal_quit = 0x40c,
            server_version_outdated = 0x40d,
            server_already_joined = 0x40d,
            server_is_not_shutting_down = 0x40e,
            server_max_vs_reached = 0x40f,
            server_unbound = 0x410,
            server_join_rate_limit_reached = 0x411,

            sql = 0x500,
            database_empty_result = 0x501,
            database_duplicate_entry = 0x502,
            database_no_modifications = 0x503,
            database_constraint = 0x504,
            database_reinvoke = 0x505,

            parameter_quote = 0x600,
            parameter_invalid_count = 0x601,
            parameter_invalid = 0x602,
            parameter_not_found = 0x603,
            parameter_convert = 0x604,
            parameter_invalid_size = 0x605,
            parameter_missing = 0x606,
            parameter_checksum = 0x607,
            parameter_constraint_violation = 0x6010,

            vs_critical = 0x700,
            connection_lost = 0x701,
            not_connected = 0x702,
            no_cached_connection_info = 0x703,
            currently_not_possible = 0x704,
            failed_connection_initialisation = 0x705,
            could_not_resolve_hostname = 0x706,
            invalid_server_connection_handler_id = 0x707,
            could_not_initialise_input_client = 0x708,
            clientlibrary_not_initialised = 0x709,
            serverlibrary_not_initialised = 0x70a,

            whisper_too_many_targets = 0x70b,
            whisper_no_targets = 0x70c,

            file_invalid_name = 0x800,
            file_invalid_permissions = 0x801,
            file_already_exists = 0x802,
            file_not_found = 0x803,
            file_io_error = 0x804,
            file_invalid_transfer_id = 0x805,
            file_invalid_path = 0x806,
            file_no_files_available = 0x807,
            file_overwrite_excludes_resume = 0x808,
            file_invalid_size = 0x809,
            file_already_in_use = 0x80a,
            file_could_not_open_connection = 0x80b,
            file_no_space_left_on_device = 0x80c,
            file_exceeds_file_system_maximum_size = 0x80d,
            file_transfer_connection_timeout = 0x80e,
            file_connection_lost = 0x80f,
            file_exceeds_supplied_size = 0x810,
            file_transfer_complete = 0x811,
            file_transfer_canceled = 0x812,
            file_transfer_interrupted = 0x813,
            file_transfer_server_quota_exceeded = 0x814,
            file_transfer_client_quota_exceeded = 0x815,
            file_transfer_reset = 0x816,
            file_transfer_limit_reached = 0x817,

            file_api_timeout = 0x820,
            file_virtual_server_not_registered = 0x821,
            file_server_transfer_limit_reached = 0x822,
            file_client_transfer_limit_reached = 0x823,

            server_insufficeient_permissions = 0xa08,
            accounting_slot_limit_reached = 0xb01,
            server_connect_banned = 0xd01,
            ban_flooding = 0xd03,

            token_invalid_id = 0xf00,
            token_expired = 0xf10,
            token_use_limit_exceeded = 0xf11,

            web_handshake_invalid = 0x1000,
            web_handshake_unsupported = 0x1001,
            web_handshake_identity_unsupported = 0x1002,
            web_handshake_identity_proof_failed = 0x1003,
            web_handshake_identity_outdated = 0x1004,

            music_invalid_id = 0x1100,
            music_limit_reached = 0x1101,
            music_client_limit_reached = 0x1102,
            music_invalid_player_state = 0x1103,
            music_invalid_action = 0x1104,
            music_no_player = 0x1105,
            music_disabled = 0x1105,
            playlist_invalid_id = 0x2100,
            playlist_invalid_song_id = 0x2101,
            playlist_already_in_use = 0x2102,
            playlist_is_in_use = 0x2103,
            query_not_exists = 0x1200,
            query_already_exists = 0x1201,

            group_invalid_id = 0x1300,
            group_name_inuse = 0x1301,
            group_not_assigned_over_this_server = 0x1302,
            group_not_empty = 0x1303,

            conversation_invalid_id = 0x2200,
            conversation_more_data = 0x2201,
            conversation_is_private = 0x2202,
            conversation_not_exists = 0x2203,

            rtc_missing_target_channel = 0x2300,

            broadcast_invalid_id = 0x2400,
            broadcast_invalid_type = 0x2401,
            broadcast_invalid_client = 0x2402,

            custom_error = 0xffff
        };
    };

    struct detailed_command_result {
        error::type error_id{};
        std::map<std::string, std::string> extra_properties{};
    };

    enum struct command_result_type {
        error       = 0b00, /* must be 0 because 0 is the default value! */
        detailed    = 0b01,
        bulked      = 0b11
    };

    /*
     * return command_result{permission::b_virtualserver_select_godmode}; => movabs rax,0xa08001700000001; ret; (Only if there is no destructor!)
     * return command_result{permission::b_virtualserver_select_godmode}; => movabs rax,0xa08001700000001; ret; (Only if there is no destructor!)
     * return command_result{error::vs_critical, "unknown error"}; => To a lot of code
     */
    struct command_result_bulk;
    struct command_result { /* fixed size of 8 (64 bits) */
        static constexpr uint64_t MASK_ERROR = ~((uint64_t) 1 << (sizeof(error::type) * 8));
        static constexpr uint64_t MASK_PERMISSION = ~((uint64_t) 1 << (sizeof(permission::PermissionType) * 8));
        static constexpr uint64_t MASK_PTR = ~((uint64_t) 0b111U);

        static constexpr uint8_t OFFSET_ERROR = (8 - sizeof(error::type)) * 8;
        static constexpr uint8_t OFFSET_PERMISSION = (8 - sizeof(permission::PermissionType) - sizeof(error::type)) * 8;
        /*
         * First bit is a flag bit which switches between detailed and code mode.
         * 0 means detailed (Because then we could interpret data as a ptr (all ptr are 8 bit aligned => 3 zero bits))
         *      bits [0;64] => data ptr (needs to be zero at destruction to avoid memory leaks)
         *
         * 1 means code
         *      bits [64 - sizeof(error::type);64] => error type | Usually evaluates to [48;64]
         *      bits [64 - sizeof(error::type) - sizeof(permission::PermissionType);64 - sizeof(error::type)] => permission id | Usually evaluates to [32;48]
         */
        uint64_t data{0};

        /* Test for mode 1 as described before */
        static_assert(sizeof(permission::PermissionType) * 8 + sizeof(error::type) * 8 <= 60);

        [[nodiscard]] inline command_result_type type() const {
            return (command_result_type) (this->data & 0b11U);
        }

        [[nodiscard]] inline bool has_error() const {
            switch (this->type()) {
                case command_result_type::bulked:
                    for(const auto& bulk : *this->bulks()) {
                        if(bulk.has_error()) {
                            return true;
                        }
                    }

                    return false;
                case command_result_type::error:
                    return this->error_code() != error::ok;
                case command_result_type::detailed:
                    return this->details()->error_id != error::ok;

                default:
                    assert(false);
                    return false;
            }
        }

        /* only valid for command_result_type::error */
        [[nodiscard]] inline error::type error_code() const {
            assert(this->type() == command_result_type::error);
            return (error::type) ((this->data >> OFFSET_ERROR) & MASK_ERROR);
        }

        /* only valid for command_result_type::error */
        [[nodiscard]] inline bool is_permission_error() const {
            assert(this->type() == command_result_type::error);
            return this->error_code() == error::server_insufficeient_permissions;
        }

        /* only valid for command_result_type::error */
        [[nodiscard]] inline permission::PermissionType permission_id() const {
            assert(this->type() == command_result_type::error);
            return (permission::PermissionType) ((this->data >> OFFSET_PERMISSION) & MASK_PERMISSION);
        }

        /* only valid for command_result_type::detailed */
        [[nodiscard]] inline const detailed_command_result* details() const {
            assert(this->type() == command_result_type::detailed);
            return (detailed_command_result*) (this->data & MASK_PTR);
        }

        /* only valid for command_result_type::detailed */
        [[nodiscard]] inline detailed_command_result* details() {
            assert(this->type() == command_result_type::detailed);
            return (detailed_command_result*) (this->data & MASK_PTR);
        }

        /* only valid for command_result_type::bulked */
        [[nodiscard]] inline const std::vector<command_result>* bulks() const {
            assert(this->type() == command_result_type::bulked);
            return (std::vector<command_result>*) (this->data & MASK_PTR);
        }

        /* only valid for command_result_type::bulked */
        [[nodiscard]] inline std::vector<command_result>* bulks() {
            assert(this->type() == command_result_type::bulked);
            return (std::vector<command_result>*) (this->data & MASK_PTR);
        }

#ifdef COMMAND_BUILDER_DEFINED
        void build_error_response(ts::command_builder& /* result */, const std::string_view& /* id key */) const;
#endif
        inline command_result& reset(command_result&& other) {
            this->release_data();
            std::exchange(this->data, other.data);
            return *this;
        }

        inline void release_data() {
            auto type = this->type();
            if(type == command_result_type::bulked) {
                auto bulks = this->bulks();
                for(auto& bulk : *bulks) {
                    bulk.release_data();
                }
                delete bulks;
            } else if(type == command_result_type::detailed) {
                auto details = this->details();
                delete details;
            }
            this->data = 0;
        }

        command_result() = default;
        command_result(const command_result&) = delete;
        command_result(command_result&& other) noexcept {
            /* we've to specify a move constructor since, we're a "trivial" type, which means that our "data" will just get copied */
            std::swap(this->data, other.data);
        }

        explicit command_result(permission::PermissionType permission) {
            this->data = (uint64_t) command_result_type::error;

            this->data |= (uint64_t) error::server_insufficeient_permissions << OFFSET_ERROR;
            this->data |= (uint64_t) permission << OFFSET_PERMISSION;
        }


        explicit command_result(error::type error) {
            this->data = (uint64_t) command_result_type::error;

            this->data |= (uint64_t) error << (8 - sizeof(error::type)) * 8;
        }

        command_result(error::type error, const std::string& message) {
            auto details_ptr = new detailed_command_result{};
            assert(((uintptr_t) details_ptr & 0x03U) == 0); // must be aligned!

            this->data = (uintptr_t) details_ptr;
            this->data |= (uint64_t) command_result_type::detailed;

            details_ptr->error_id = error;
            details_ptr->extra_properties["extra_msg"] = message;
        }

        command_result(error::type error, const std::map<std::string, std::string>& properties) : command_result{error, std::string{""}} {
            assert(this->type() == command_result_type::detailed);
            this->details()->extra_properties = properties;
        }

        explicit command_result(command_result_bulk&&);

#if !defined(_NDEBUG) && false
        /* if we're not using any debug we dont have to use a destructor. A destructor prevent a direct uint64_t return as described above */
        ~command_result() {
            assert(this->data == 0);
        }
#endif
    };
    static_assert(sizeof(command_result) == 8);

    struct command_result_bulk {
        friend struct command_result;
        public:
            command_result_bulk() = default;
            explicit command_result_bulk(command_result&& result) { this->results.push_back(std::forward<command_result>(result)); }

            ~command_result_bulk() {
                for(auto& result : this->results) {
                    result.release_data();
                }
            }

            inline size_t length() const {
                return this->results.size();
            }

            inline const ts::command_result& response(size_t index) const {
                return this->results[index];
            }

            inline void reserve(size_t length) {
                this->results.reserve(length);
            }

            inline void insert_result(ts::command_result&& result) {
                this->results.push_back(std::forward<ts::command_result>(result));
            }

            inline void set_result(size_t index, ts::command_result&& result) {
                auto& result_container = this->results[index];
                result_container.reset(std::forward<ts::command_result>(result));
            }

            inline void emplace_result(permission::PermissionType permission) {
                this->results.emplace_back(permission);
            }

            inline void emplace_result(error::type error) {
                this->results.emplace_back(error);
            }

            inline void emplace_result(error::type error, const std::string& message) {
                this->results.emplace_back(error, message);
            }

            template <typename... Args>
            inline void emplace_result_n(size_t times, const Args&&... args) {
                while(times-- > 0)
                    this->results.emplace_back(args...);
            }

            [[nodiscard]] inline auto begin() { return this->results.begin(); }
            [[nodiscard]] inline auto end() { return this->results.end(); }
            [[nodiscard]] inline auto cbegin() const { return this->results.cbegin(); }
            [[nodiscard]] inline auto cend() const { return this->results.cend(); }
        private:
            std::vector<command_result> results{};
    };

    inline command_result::command_result(ts::command_result_bulk &&bulk) {
        auto bulks = new std::vector<command_result>{};
        assert(((uintptr_t) bulks & 0x03U) == 0); // must be aligned!

        this->data = (uintptr_t) bulks;
        this->data |= (uint64_t) command_result_type::bulked;

        bulks->swap(bulk.results);
    }

    struct ErrorType {
        public:
            static ErrorType Success;
            static ErrorType Costume;
            static ErrorType VSError;
            static ErrorType DBEmpty;

            ErrorType(const uint16_t errorId, std::string name, std::string message) : errorId(errorId), name(std::move(name)), message(std::move(message)) {}
            ErrorType(const ErrorType& ref) = default;
            ErrorType(ErrorType&& ref) : errorId(ref.errorId), name(std::move(ref.name)), message(std::move(ref.message)) {}

            uint16_t errorId;
            std::string name;
            std::string message;

            bool operator==(const ErrorType& ref) const {
                return this->errorId == ref.errorId;
            }

            bool operator!=(const ErrorType& ref) const { return !operator==(ref); }

            ErrorType& operator=(const ErrorType& ref) {
                this->errorId = ref.errorId;
                this->name = ref.name;
                this->message = ref.message;
                return *this;
            }

            /**
             * @return true if fail
             */
            bool operator!() const {
                return errorId != 0;
            }
    };

    extern const std::vector<ErrorType> availableErrors;
    extern ErrorType findError(uint16_t errorId);
    extern ErrorType findError(std::string key);
}