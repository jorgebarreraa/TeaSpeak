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

#define _NDEBUG

namespace ts {
    struct CommandResult;
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
            server_insufficeient_permissions = 0xa08,
            accounting_slot_limit_reached = 0xb01,
            server_connect_banned = 0xd01,
            ban_flooding = 0xd03,
            token_invalid_id = 0xf00,
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

            conversation_invalid_id = 0x2200,
            conversation_more_data = 0x2201,
            conversation_is_private = 0x2202,

            custom_error = 0xffff
        };
    };

    struct detailed_command_result {
        error::type error_id;
        std::map<std::string, std::string> extra_properties;
    };


    /*
     * return command_result{permission::b_virtualserver_select_godmode}; => movabs rax,0xa08001700000001; ret; (Only if there is no destructor!)
     * return command_result{permission::b_virtualserver_select_godmode}; => movabs rax,0xa08001700000001; ret; (Only if there is no destructor!)
     * return command_result{error::vs_critical, "unknown error"}; => To a lot of code
     */
    struct command_result { /* fixed size of 8 (64 bits) */
        static constexpr uint64_t MASK_ERROR = ~((uint64_t) 1 << (sizeof(error::type) * 8));
        static constexpr uint64_t MASK_PERMISSION = ~((uint64_t) 1 << (sizeof(permission::PermissionType) * 8));

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
        uint64_t data = 0;

        /* Test for mode 1 as described before */
        static_assert(sizeof(permission::PermissionType) * 8 + sizeof(error::type) * 8 <= 62);

        [[nodiscard]] inline error::type error_code() const {
            if(this->is_detailed()) return this->details()->error_id;

            return (error::type) ((this->data >> OFFSET_ERROR) & MASK_ERROR);
        }

        [[nodiscard]] inline bool is_permission_error() const {
            return this->error_code() == error::server_insufficeient_permissions;
        }

        [[nodiscard]] inline permission::PermissionType permission_id() const {
            if(this->is_detailed()) return (permission::PermissionType) -1; /* not supported */

            return (permission::PermissionType) ((this->data >> OFFSET_PERMISSION) & MASK_PERMISSION);
        }

        [[nodiscard]] inline const detailed_command_result* details() const { return (detailed_command_result*) this->data; }
        [[nodiscard]] inline detailed_command_result* details() { return (detailed_command_result*) this->data; }

        [[nodiscard]] inline bool is_detailed() const {
            return (this->data & 0x1UL) == 0;
        }

        inline std::unique_ptr<detailed_command_result> release_details() {
            if(!this->is_detailed()) return nullptr;

            auto result = this->details();
            this->data = 0;
            return std::unique_ptr<detailed_command_result>{result};
        }

        /* Attention: Releases the internal detailed pointer! */
        inline CommandResult as_command_result();

#ifndef _NDEBUG /* We dont need to secure that because gcc deduct us to an uint64_t and the only advantage is the mem leak test which is deactivated anyways */
        command_result(command_result&) = delete;
        command_result(const command_result&) = delete;
        command_result(command_result&& other) : data(other.data) {
            other.data = 0;
        }
#endif

        command_result() = default;

        explicit command_result(permission::PermissionType permission) {
            this->data = 0x01; /* the the type to 1 */

            this->data |= (uint64_t) error::server_insufficeient_permissions << OFFSET_ERROR;
            this->data |= (uint64_t) permission << OFFSET_PERMISSION;
        }


        explicit command_result(error::type error) {
            this->data = 0x01; /* the the type to 1 */

            this->data |= (uint64_t) error << (8 - sizeof(error::type)) * 8;
        }

        command_result(error::type error, const std::string& message) {
            auto details_ptr = new detailed_command_result{};
            assert(((uintptr_t) details_ptr & 0x03U) == 0); // must be aligned!

            this->data = (uintptr_t) details_ptr;

            details_ptr->error_id = error;
            details_ptr->extra_properties["extra_msg"] = message;
        }

        command_result(error::type error, const std::map<std::string, std::string>& properties) : command_result{error, std::string{""}} {
            assert(this->is_detailed());
            this->details()->extra_properties = properties;
        }

#ifndef _NDEBUG
        /* if we're not using any debug we dont have to use a deconstructor. A deconstructor prevent a direct uint64_t return as described above */
        ~command_result() {
            if((this->data & 0x01) == 0x00) {
                // this->details needs to be removed 'till this gets destructed
                assert(this->data == 0);
            }
        }
#endif
    };
    static_assert(sizeof(command_result) == 8);

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
                return errorId == ref.errorId;
            }

            bool operator!=(const ErrorType& ref) const { return !operator==(ref); }

            ErrorType& operator=(const ErrorType& ref) {
                errorId = ref.errorId;
                name = ref.name;
                message = ref.message;
                return *this;
            }

            /**
             * @return true if fail
             */
            bool operator!() const {
                return errorId != 0;
            }
    };

    extern const std::vector<ErrorType> avariableErrors;
    extern ErrorType findError(uint16_t errorId);
    extern ErrorType findError(std::string key);

    enum CommandResultType {
        GENERAL,
        PERM_ERROR
    };

    struct CommandResult {
        public:
            static CommandResult Success;
            static CommandResult NotImplemented;

            CommandResult(const CommandResult& ref) : _type(ref._type), error(ref.error), extraProperties(ref.extraProperties) {}
            CommandResult(CommandResult&& ref) : _type(ref._type), error(ref.error), extraProperties(ref.extraProperties) {}
            CommandResult(ErrorType error, const std::string &extraMsg = "") : error(std::move(error)) { if(extraMsg.empty()) return; /*extraProperties["extramsg"] = extraMsg; */extraProperties["extra_msg"] = extraMsg; }
            CommandResult(std::string error, const std::string &extraMsg = "") : error(findError(std::move(error))) { if(extraMsg.empty()) return; /*extraProperties["extramsg"] = extraMsg; */extraProperties["extra_msg"] = extraMsg; }
            CommandResult() : CommandResult(ErrorType::Success, ""){}
            CommandResult(ErrorType error, std::map<std::string, std::string> details) : error(error), extraProperties(std::move(details)) {}

            bool operator==(const CommandResult& ref){
                return this->error == ref.error && ref.extraProperties == this->extraProperties;
            }

            CommandResult& operator=(const CommandResult& ref)= default;

            /**
            * @return true if fail
            */
            bool operator!() const {
                return this->error != ErrorType::Success;
            }
            virtual CommandResultType type(){ return _type; }

            ErrorType error;
            std::map<std::string, std::string> extraProperties;
            CommandResultType _type = CommandResultType::GENERAL;
    };

    struct CommandResultPermissionError : public CommandResult {
        public:
            CommandResultPermissionError(permission::PermissionType error, const std::string &extraMsg = "");
    };

    CommandResult command_result::as_command_result() {
        if(this->is_detailed()) {
            const auto details = this->details();
            auto result = CommandResult{findError(details->error_id), details->extra_properties};
            this->release_details();
            return result;
        } else {
            const auto code = this->error_code();
            auto error = findError(this->error_code());
            if(code == error::server_insufficeient_permissions)
                return CommandResultPermissionError{(permission::PermissionType) this->permission_id()};
            else
                return CommandResult{error};
        }
    }
}

#undef _NDEBUG