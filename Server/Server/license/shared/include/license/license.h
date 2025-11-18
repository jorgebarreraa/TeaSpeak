#pragma once

#include <memory>
#include <string>
#include <chrono>
#include <memory>
#include <utility>
#include <ThreadPool/Mutex.h>
#include <ThreadPool/Future.h>
#include <Variable.h>

#define LICENSE_VERSION 1
#define LICENSE_PROT_VERSION 3
#define MAGIC_NUMER 0xBADC0DED

namespace license {
    namespace exceptions {
        class LicenseException : public std::exception {
            public:
                LicenseException() = delete;
                explicit LicenseException(std::string message) : errorMessage(std::move(message)) {}
                LicenseException(const LicenseException& ref) : errorMessage(ref.errorMessage) {}
                explicit LicenseException(LicenseException&& ref) : errorMessage(std::move(ref.errorMessage)) {}

                [[nodiscard]] const char* what() const noexcept override;
            private:
                std::string errorMessage;
        };
    }

    struct LicenseHeader {
        uint16_t version;
        const char cryptKey[64]; //The dummy key for data de/encryption
    } __attribute__ ((__packed__));

    namespace v2 {
    	struct LicenseHeader {
		    uint16_t version; /* first 16 bytes const version */
		    uint64_t crypt_key_seed; /* the seed */
		    uint8_t crypt_key_verify_offset; /*  the first 8 bits determine how much generations (n * 3) and the rest what value is expected. Due to the loss of 8 bits does the highest 8 bits get xored with the lowerst and 56 bits get compared */
		    uint8_t crypt_key_verify[5];
    	} __attribute__ ((__packed__));
    	static_assert(sizeof(LicenseHeader) == 16);

    	struct BodyHeader {
			uint16_t length_private_data;
		    uint16_t length_hierarchy; /* contains all public data */

		    uint8_t checksum_hierarchy[20]; /* SHA1 */
		    uint8_t private_data_sign[64]; /* ed sign from the hierarchy created public key */
    	} __attribute__ ((__packed__));

    	extern std::array<uint8_t, 32> public_root_key;

    	struct HierarchyEntry;
    	struct LicensePrivate;
    	struct LicensePrivateWriteOptions;
    	struct License {
    		public:
    			static std::shared_ptr<License> read(const uint8_t* /* buffer */, size_t /* max size */, uint8_t& /* error */);
    			static std::shared_ptr<License> create(const std::vector<std::shared_ptr<const HierarchyEntry>>& hierarchy, const std::array<uint8_t, 32>& /* precalculated last entry */);
    			/* Note for the write method: Make the private version index configurable, so we could "export" the license */
    			/* Note for the write method: Write "private_buffer" if we're not able to resign the private data. As well enfore (assert) that we have a private buffer (e.g. by the read method) */

			    ~License();
			    [[nodiscard]] std::vector<std::shared_ptr<const HierarchyEntry>> hierarchy() const { return this->_hierarchy; }
				bool push_entry(const std::shared_ptr<const HierarchyEntry>& /* entry */, size_t* /* index */ = nullptr);

			    bool hierarchy_timestamps_valid();

			    template <typename I, typename... Args>
			    bool push_entry(Args&&... args) {
			    	std::array<uint8_t, 32> key_private{}, key_public{};
			    	if(!this->generate_keypair(key_private.data(), key_public.data())) return false;

			    	auto entry = I::create(key_public.data(), std::forward<Args>(args)...);
			    	if(!entry) return false;

			    	size_t index;
				    if(!this->push_entry(entry, &index)) return false;

			    	this->_register_raw_private_key(index, key_private.data());
			    	return true;
			    }

			    std::array<uint8_t, 32> generate_public_key(const uint8_t* /* public key root */, int /* length */ = -1) const;

			    std::string write(uint8_t& /* error */);

			    [[nodiscard]] bool private_data_editable() const;
			    bool write_private_data(const LicensePrivateWriteOptions& /* write options */);

			    [[nodiscard]] inline uint16_t version() const { return this->_version; }
		    private:
			    License() = default;

			    uint16_t _version = 0;

			    std::array<uint8_t, 64> private_buffer_sign{};
			    uint8_t* private_buffer = nullptr;
			    size_t private_buffer_length = 0;
			    std::shared_ptr<LicensePrivate> private_data{};

			    uint64_t crypt_seed = 0;
    	        std::vector<std::shared_ptr<const HierarchyEntry>> _hierarchy{};

    	        bool generate_keypair(uint8_t* /* private key */, uint8_t* /* public key */);
    	        void _register_raw_private_key(size_t /* index */, uint8_t* /* key */);
    	};

	    struct LicensePrivateWriteOptions {
		    int precalculated_key_index = -2; /* -2 => Do not write any private keys; -1 => Write everything */
	    };

    	struct LicensePrivate {
		    public:
    			static std::shared_ptr<LicensePrivate> read(const std::shared_ptr<License>& /* handle */, uint8_t /* version */, const uint8_t* /* buffer */, size_t /* length */, uint8_t& /* error */);
			    static std::shared_ptr<LicensePrivate> create(const std::shared_ptr<License>& /* handle */, int /* precalculated private key index */, const uint8_t* /* precalculated key */);

			    bool private_key_chain_valid();

			    [[nodiscard]] bool has_meta(const std::string& key) const { return this->meta_data.count(key) > 0; }
			    [[nodiscard]] std::string get_meta(const std::string& key) const { return this->meta_data.at(key); }
			    void set_meta(const std::string& key, const std::string& value) { this->meta_data[key] = value; }

			    /* if target is null just increase the offset! */
			    bool write(uint8_t* /* target */, size_t& /* offset */, size_t /* length */, const LicensePrivateWriteOptions& /* options */);

			    void register_raw_private_key(uint8_t /* index */, const uint8_t* /* key */);
    			[[nodiscard]] bool has_raw_private_key(uint8_t /* index */) const;

    			[[nodiscard]] bool private_key_calculable(int /* index */) const;
    			bool calculate_private_key(uint8_t* /* response */, uint8_t /* index */) const;
		    private:
    			std::weak_ptr<License> _handle;
			    LicensePrivate() = default;

			    int precalculated_private_key_index = -2; /* -2 means not set, -1 means root key */
			    std::array<uint8_t, 32> precalculated_private_key{};
			    std::map<uint8_t, std::array<uint8_t, 32>> private_keys{};
			    std::map<std::string, std::string> meta_data{};
    	};

    	namespace hierarchy {
    		struct BodyInterpreter;
    	}

    	struct HierarchyEntry {
    		friend struct hierarchy::BodyInterpreter;
    		public:
    			~HierarchyEntry();

				static std::shared_ptr<const HierarchyEntry> read(const uint8_t* /* buffer */, size_t& /* offset */, size_t /* max size */);
			    bool write(uint8_t* /* buffer */, size_t& /* offset */, size_t /* max size */) const;
			    inline size_t write_length() const { return 43 + this->read_body_length; }

			    inline uint8_t entry_type() const { return this->_entry_type; }

			    template <typename T = std::chrono::system_clock>
			    inline typename T::time_point begin_timestamp() const { return typename T::time_point{} + std::chrono::minutes(this->_timestamp_begin); }

			    template <typename T = std::chrono::system_clock>
			    inline typename T::time_point end_timestamp() const { return typename T::time_point{} + std::chrono::minutes(this->_timestamp_end); }

			    inline const std::array<uint8_t, 32>& public_key() const { return this->_public_key; }
			    inline std::array<uint8_t, 32>& public_key() {
				    this->_hash_set = false;
			    	return this->_public_key;
			    }

			    inline const uint8_t* body() const { return this->read_body; }
			    inline size_t body_length() const { return this->read_body_length; }

			    template <typename I>
			    inline I interpret_as() const {
				    assert(this->interpretable_as<I>());
				    return I{this->read_body, this->read_body_length};
			    }

			    template <typename I>
			    inline bool interpretable_as() const { return I::_type == this->_entry_type; }

			    inline bool hash(uint8_t* /* hash result [64] */) const;
		    protected:
			    template <typename T = std::chrono::system_clock>
			    HierarchyEntry(uint8_t type, const uint8_t* public_key, const typename T::time_point& begin, const typename T::time_point& end) {
			    	this->_entry_type = type;
			    	memcpy(this->_public_key.data(), public_key, this->_public_key.size());
				    this->_timestamp_begin = std::chrono::floor<std::chrono::minutes>(begin.time_since_epoch()).count();
				    this->_timestamp_end = std::chrono::floor<std::chrono::minutes>(end.time_since_epoch()).count();
			    }
		    private:
			    HierarchyEntry() = default;

			    mutable std::array<uint8_t, 64> _hash{};
			    mutable bool _hash_set = false;

			    uint8_t _entry_type = 0;
    			std::array<uint8_t, 32> _public_key{};

    			uint32_t _timestamp_begin = 0; /* Minutes since epoch! */
			    uint32_t _timestamp_end = 0; /* Minutes since epoch! */

			    uint8_t* read_body = nullptr;
			    size_t read_body_length = 0;

			    bool allocate_read_body(size_t);
    	};

    	namespace hierarchy {
    		struct type {
    			enum value : uint8_t {
    				Intermediate = 1,
    				Server = 2,
				    Ephemeral = 8
    			};

    			static constexpr const char* name(const value& value) {
    				switch (value) {
					    case Intermediate:
					    	return "Intermediate";
					    case Server:
						    return "Server";
					    case Ephemeral:
						    return "Ephemeral";

					    default:
					    	return "Unknown";
    				}
    			}
    		};
    		struct BodyInterpreter {
			    public:
				    BodyInterpreter() = delete;

    			protected:
    				template <typename T>
    				static std::shared_ptr<HierarchyEntry> _create(
    						const uint8_t *pub_key,
    						const std::chrono::system_clock::time_point &begin,
    						const std::chrono::system_clock::time_point &end,
    						size_t buffer_size, uint8_t*& buffer_ptr
                    ) {
					    auto result = std::make_shared<HierarchyEntry>(HierarchyEntry{T::_type, pub_key, begin, end});
					    if(!result || !result->allocate_read_body(buffer_size)) return nullptr;
					    result->read_body_length = buffer_size;
					    buffer_ptr = result->read_body;
    				    return result;
    				}

				    BodyInterpreter(const uint8_t* memory, size_t length) { this->_memory = memory; this->_length = length; }
				    const uint8_t* _memory = nullptr;
				    size_t _length = 0;
    		};

    		struct Intermediate : public BodyInterpreter {
    			public:
    				static constexpr uint8_t _type = type::Intermediate;
				    static std::shared_ptr<const HierarchyEntry> create(
						    const uint8_t* /* public key */,
						    const std::chrono::system_clock::time_point& /* begin */,
						    const std::chrono::system_clock::time_point& /* end */,
						    const std::string& /* description */
				    );

					std::string_view description();
    			private:
				    Intermediate(const uint8_t* memory, size_t length) : BodyInterpreter(memory, length) {}
    		};

		    struct Server : public BodyInterpreter {
			    public:
				    static constexpr uint8_t _type = type::Server;
				    static std::shared_ptr<const HierarchyEntry> create(
						    const uint8_t* /* public key */,
						    const std::chrono::system_clock::time_point& /* begin */,
						    const std::chrono::system_clock::time_point& /* end */,

						    const std::string& /* email */,
						    const std::optional<std::string>& /* value */
				    );

				    std::string_view contact_email();

				    bool has_username();
				    std::string_view username();
			    private:
				    Server(const uint8_t* memory, size_t length) : BodyInterpreter(memory, length) {}
		    };

		    struct Ephemeral : public BodyInterpreter {
			    public:
				    static constexpr uint8_t _type = type::Ephemeral;
				    static std::shared_ptr<const HierarchyEntry> create(
						    const uint8_t* /* public key */,
						    const std::chrono::system_clock::time_point& /* begin */,
						    const std::chrono::system_clock::time_point& /* end */
				    );
			    private:
				    Ephemeral(const uint8_t* memory, size_t length) : BodyInterpreter(memory, length) {}
		    };
    	}
    }

    enum LicenseType : uint8_t {
        INVALID,
        DEMO,
        PREMIUM,
        HOSTER,
        PRIVATE,
    };

    inline bool isPremiumLicense(LicenseType type){ return type == HOSTER || type == PREMIUM || type == PRIVATE; }

    #define LT_NAMES {"Invalid", "Demo", "Premium", "Hoster", "Private"};
    extern std::string LicenseTypeNames[5];

    struct License {
        LicenseHeader header;

        //Crypted part
        struct {
            LicenseType type;
            int64_t endTimestamp;

            const char licenceKey[64]; //The actual key
            const char licenceOwner[64];

        } __attribute__ ((__packed__)) data;

	    inline std::string key() { return std::string(data.licenceKey, 64); }
	    inline std::chrono::time_point<std::chrono::system_clock> end() { return std::chrono::system_clock::time_point() + std::chrono::milliseconds(this->data.endTimestamp); }
	    inline std::string owner() { return std::string(this->data.licenceOwner); } //Scopetita
        inline bool isValid() { return data.endTimestamp == 0 || std::chrono::system_clock::now() < this->end(); }
        inline bool isPremium(){ return isPremiumLicense(data.type); }
    } __attribute__ ((__packed__));

    struct LicenseInfo {
        LicenseType type;
	    std::string username;
	    std::string first_name;
        std::string last_name;
        std::string email;
        std::chrono::system_clock::time_point start;
	    std::chrono::system_clock::time_point end;
	    std::chrono::system_clock::time_point creation;

	    bool deleted{false};
	    uint64_t upgrade_id{0};

        inline bool isNotExpired() { return (end.time_since_epoch().count() == 0 || std::chrono::system_clock::now() < this->end); }
    };

    extern std::shared_ptr<License> readLocalLicence(const std::string &, std::string &);
    extern std::string exportLocalLicense(const std::shared_ptr<License>&);
    extern std::string createLocalLicence(LicenseType type, std::chrono::time_point<std::chrono::system_clock> until, std::string licenseOwner);

	namespace protocol {
		enum RequestState {
			UNCONNECTED,
			CONNECTING,
			
			HANDSCHAKE,
			SERVER_VALIDATION,
			LICENSE_INFO,

			PROPERTY_ADJUSTMENT,
			LICENSE_UPGRADE,

			MANAGER_AUTHORIZATION,
			MANAGER_CONNECTED,

			DISCONNECTING
		};

		enum PacketType : uint8_t {
			PACKET_CLIENT_HANDSHAKE,
			PACKET_SERVER_HANDSHAKE,

			PACKET_CLIENT_SERVER_VALIDATION,
			PACKET_SERVER_VALIDATION_RESPONSE,

			PACKET_CLIENT_PROPERTY_ADJUSTMENT,
			PACKET_SERVER_PROPERTY_ADJUSTMENT,

			PACKET_CLIENT_AUTH_REQUEST,
			PACKET_SERVER_AUTH_RESPONSE,
			PACKET_CLIENT_LICENSE_CREATE_REQUEST,
			PACKET_SERVER_LICENSE_CREATE_RESPONSE,

            PACKET_CLIENT_LIST_REQUEST,
            PACKET_SERVER_LIST_RESPONSE,

			PACKET_CLIENT_DELETE_REQUEST,
			PACKET_CLIENT_DELETE_RESPONSE,

			PACKET_CLIENT_LICENSE_UPGRADE,
			PACKET_SERVER_LICENSE_UPGRADE_RESPONSE,

			PACKET_PING = 0xF0,
			PACKET_DISCONNECT = 0xFF
		};

		struct packet_header {
			PacketType packetId{0};
			uint16_t length{0};
		};

		struct packet {
			struct {
				PacketType packetId{0};
				mutable uint16_t length{0};
			} header;
			std::string data;

			inline void prepare() const {
				this->header.length = (uint16_t) data.length();
			}

			packet(PacketType packetId, std::string data) : data(std::move(data)), header({packetId, 0}) {}

#ifdef GOOGLE_PROTOBUF_MESSAGE_H__
			packet(PacketType packetId, const ::google::protobuf::Message&);
#endif
			packet(PacketType packetId, std::nullptr_t);
		};
	}
}
//DEFINE_TRANSFORMS(license::LicenseType, uint8_t);