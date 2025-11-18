#include <google/protobuf/message.h>
#include <misc/base64.h>
#include <random>
#include <ed25519/ed25519.h>

#define NO_OPEN_SSL
#include <misc/digest.h>
#include <cstring>
#include <cassert>
#include <ed25519/ge.h>
#include <ed25519/sc.h>
#include "crypt.h"
#include "shared/include/license/license.h"

using namespace std;
using namespace std::chrono;

inline void generate(char* buffer, size_t length){
    for(int index = 0; index < length; index++)
        buffer[index] = (uint8_t) rand();
}

namespace license {
    std::string LicenseTypeNames[] = LT_NAMES;

     std::shared_ptr<License> readLocalLicence(const std::string& buffer, std::string& error){
        string bbuffer = base64::decode(buffer);
        if(bbuffer.length() < sizeof(License)) {
            error = "invalid size";
            return nullptr;
        }

        auto license = static_cast<License *>(malloc(sizeof(License)));
        memcpy(license, bbuffer.data(), sizeof(License));

        if(license->header.version != LICENSE_VERSION){
            error = "invalid version (" + to_string(license->header.version) + ")";
            return nullptr;
        }
        xorBuffer(&((char*) license)[sizeof(License::header)], sizeof(License::data), license->header.cryptKey, sizeof(license->header.cryptKey));

        auto hash = digest::sha1(reinterpret_cast<const char *>(&license->data), sizeof(license->data));

        uint64_t checkSum = 0;
        for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
            checkSum += (uint8_t) hash[i] << (i % 8);

        if((checkSum ^ *(uint64_t*) &license->header.cryptKey) != MAGIC_NUMER) {
            error = "invalid check sum";
            return nullptr;
        }

        return shared_ptr<License>(license, [](License* l){
            if(l) free(l);
        });
    }

	std::string exportLocalLicense(const std::shared_ptr<License>& ref){
        auto copy = static_cast<License *>(malloc(sizeof(License)));
        memcpy(copy, ref.get(), sizeof(License));

        auto hash = digest::sha1(reinterpret_cast<const char *>(&copy->data), sizeof(copy->data));

        uint64_t checkSum = 0;
        for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
            checkSum += (uint8_t) hash[i] << (i % 8);
        checkSum ^= MAGIC_NUMER;

        generate(const_cast<char *>(copy->header.cryptKey), sizeof(copy->header.cryptKey));
        *(uint64_t*) &copy->header.cryptKey = checkSum;


        xorBuffer(&((char*) copy)[sizeof(License::header)], sizeof(License::data), copy->header.cryptKey, sizeof(copy->header.cryptKey));
        auto result = base64_encode((const char*) copy, sizeof(License));
        free(copy);
        return result;
    }

	std::string createLocalLicence(LicenseType type, std::chrono::system_clock::time_point until, std::string licenseOwner){
        auto license = shared_ptr<License>(static_cast<License *>(malloc(sizeof(License))), [](License* l) { if(l) free(l); });
        assert(licenseOwner.length() < sizeof(license->data.licenceOwner));

        license->header.version = LICENSE_VERSION;
        generate(const_cast<char *>(license->data.licenceKey), sizeof(license->data.licenceKey));
        generate(const_cast<char *>(license->data.licenceOwner), sizeof(license->data.licenceOwner)); //Crap data :)
        license->data.type = type;
        license->data.endTimestamp = duration_cast<milliseconds>(until.time_since_epoch()).count();
        memcpy((void *) license->data.licenceOwner, licenseOwner.c_str(), strlen(licenseOwner.c_str()) + 1); //Copy the string into it

        return exportLocalLicense(license);
    }

    const char *exceptions::LicenseException::what() const throw() {
        return this->errorMessage.c_str();
    }

	protocol::packet::packet(PacketType packetId, const ::google::protobuf::Message& message) {
		this->header.packetId = packetId;
		this->data = message.SerializeAsString();
	}

	protocol::packet::packet(license::protocol::PacketType packetId, nullptr_t) {
        this->header.packetId = packetId;
        this->data = "";
    }
}

std::array<uint8_t, 32> license::v2::public_root_key = {
	0x84, 0x54, 0x2c, 0x2b, 0x46, 0x19, 0x05, 0x6c,
	0x01, 0xd8, 0x61, 0x49, 0x4e, 0x48, 0x47, 0x1e,
	0x6c, 0x61, 0xfa, 0x6a, 0xde, 0x6b, 0x1c, 0x76,
	0x3a, 0xeb, 0x2f, 0x39, 0x49, 0x3d, 0x71, 0x35
};

namespace license::v2 {
	License::~License() {
		if(this->private_buffer)
			::free(this->private_buffer);
	}

	std::shared_ptr<License> License::create(const std::vector<std::shared_ptr<const license::v2::HierarchyEntry>> &hierarchy, const std::array<uint8_t, 32> &prv_key) {
		auto result = shared_ptr<License>(new License{});

		result->_version = 2;
		result->crypt_seed = std::mt19937_64{std::random_device{}()}();
		result->_hierarchy = hierarchy;
		result->private_data = LicensePrivate::create(result, (int) hierarchy.size() - 1, prv_key.data());

		return result;
	}

	std::shared_ptr<License> License::read(const uint8_t *buffer, size_t length, uint8_t &error) {
		auto result = shared_ptr<License>(new License{});

		LicenseHeader header{};
		BodyHeader body_header{};
		if(length < sizeof(header) + sizeof(body_header)) {
			error = 2; /* buffer too small */
			return nullptr;
		}
		memcpy(&header, buffer, sizeof(header));

		if(header.version != 2) {
			error = 3; /* invalid version */
			return nullptr;
		}
		result->_version = header.version;

		std::mt19937_64 crypt_key_gen{header.crypt_key_seed};

		/* verify the crypt key gen */
		{
			crypt_key_gen.discard(header.crypt_key_verify_offset);
			uint64_t expected = 0;
			memcpy(&expected, header.crypt_key_verify, 5);

			uint64_t received = crypt_key_gen();
			received = received ^ (received >> 40UL);
			received &= 0xFFFFFFFFFFULL;

			if(expected != received) {
				error = 4; /* invalid key sequence */
				return nullptr;
			}
		}

		crypt_key_gen.seed(header.crypt_key_seed);
		auto decoded_buffer_length = length - sizeof(header);
		auto decoded_buffer = unique_ptr<uint8_t, decltype(::free)*>{(uint8_t*) malloc(decoded_buffer_length), ::free};
		if(!decoded_buffer) {
			error = 1; /* out of memory */
			return nullptr;
		}

		/* "decode" the data */
		{
			auto index = sizeof(header);
			auto index_decoded = 0;
			while(index + 4 < length) {
				auto& memory = *(uint32_t*) (&*decoded_buffer + index_decoded);
				memory = *(uint32_t*) (buffer + index);
				memory ^= (uint32_t) crypt_key_gen();
				index += 4;
				index_decoded += 4;
			}
			while(index < length) {
				auto& memory = *(uint8_t*) (&*decoded_buffer + index_decoded);
				memory = *(uint8_t*) (buffer + index);
				memory ^= (uint8_t) crypt_key_gen();
				index++;
				index_decoded++;
			}
		}

		memcpy(&body_header, &*decoded_buffer, sizeof(body_header));
		if(decoded_buffer_length < sizeof(body_header) + body_header.length_hierarchy + body_header.length_private_data) {
			error = 2; /* buffer too small */
			return nullptr;
		}

		auto hierarchy_buffer = &*decoded_buffer + sizeof(body_header) + body_header.length_private_data;
		/* test the checksum for the hierarchy (license data indirectly verified via data_sign) */
		{
			uint8_t sha_buffer[20];
			digest::sha1((char*) hierarchy_buffer, body_header.length_hierarchy, sha_buffer);
			if(memcmp(sha_buffer, body_header.checksum_hierarchy, 20) != 0) {
				error = 5; /* checksum does not match */
				return nullptr;
			}
		}

		/* now lets read the hierarchy data */
		{
			size_t offset = 0, length = body_header.length_hierarchy;
			while(offset < length) {
				auto entry = HierarchyEntry::read(hierarchy_buffer, offset, length);
				if(!entry) {
					error = 6; /* failed to read an entry */
					return nullptr;
				}

				result->_hierarchy.push_back(entry);
			}
		}

		/* verify the given data */
		auto public_key = result->generate_public_key(public_root_key.data());
		if(!ed25519_verify(body_header.private_data_sign, &*decoded_buffer + sizeof(body_header), body_header.length_private_data, public_key.data())) {
			error = 7; /* failed to verify private data */
			return nullptr;
		}

		memcpy(result->private_buffer_sign.data(), body_header.private_data_sign, 64);

		/* copy the private data */
		result->private_buffer = (uint8_t*) malloc(body_header.length_private_data);
		result->private_buffer_length = body_header.length_private_data;
		memcpy(result->private_buffer, &*decoded_buffer + sizeof(body_header), body_header.length_private_data);

		/* let parse the private data */
		result->private_data = LicensePrivate::read(result, result->_version, result->private_buffer, result->private_buffer_length, error);
		if(!result->private_data) {
			error = 7; /* failed to parse private data */
			return nullptr;
		}
		return result;
	}

	std::string License::write(uint8_t &error) {
		if(!this->private_data || !this->private_buffer_length) {
			error = 2; /* missing private data */
			return "";
		}

		/* lets estimate a buffer size */
		auto buffer_size = sizeof(LicenseHeader) + sizeof(BodyHeader) + this->private_buffer_length;

		for(auto& he : this->_hierarchy)
			if(!he->write(nullptr, buffer_size, 0)) {
				error = 3; /* failed to estimate buffer size */
				return "";
			}

		auto buffer = unique_ptr<uint8_t, decltype(::free)*>{(uint8_t*) malloc(buffer_size), ::free};
		LicenseHeader license_header{};
		BodyHeader body_header{};

		/* first copy the private data */
		{
			memcpy(body_header.private_data_sign, this->private_buffer_sign.data(), this->private_buffer_sign.size());
			memcpy(&*buffer + sizeof(license_header) + sizeof(body_header), this->private_buffer, this->private_buffer_length);
			body_header.length_private_data = this->private_buffer_length;
		}

		/* lets write the hierarchy */
		{
			auto offset = sizeof(license_header) + sizeof(body_header) + this->private_buffer_length;
			const auto begin_offset = offset;
			for(auto& he : this->_hierarchy)
				if(!he->write(&*buffer, offset, buffer_size)) {
					error = 3; /* failed to write hierarchy */
					return "";
				}

			body_header.length_hierarchy = offset - begin_offset;
			digest::sha1((char*) &*buffer + begin_offset, body_header.length_hierarchy, body_header.checksum_hierarchy);
		}

		/* write the body header */
		memcpy(&*buffer + sizeof(license_header), &body_header, sizeof(body_header));

		/* lets generate the license header */
		{
			std::mt19937_64 rnd{std::random_device{}()};

			license_header.version = 2;
			license_header.crypt_key_seed = rnd();
			license_header.crypt_key_verify_offset = std::uniform_int_distribution<uint8_t>{}(rnd);

			{
				rnd.seed(license_header.crypt_key_seed);
				rnd.discard(license_header.crypt_key_verify_offset);

				uint64_t expected = rnd();
				expected = expected ^ (expected >> 40UL);
				expected &= 0xFFFFFFFFFFULL;

				memcpy(license_header.crypt_key_verify, &expected, 5);
			}
		}

		/* now lets "encrypt" the body */
		{
			std::mt19937_64 crypt_key_gen{license_header.crypt_key_seed};

			auto index = sizeof(license_header);
			while(index + 4 < buffer_size) {
				auto& memory = *(uint32_t*) (&*buffer + index);
				memory = *(uint32_t*) (&*buffer + index);
				memory ^= (uint32_t) crypt_key_gen();
				index += 4;
			}
			while(index < buffer_size) {
				auto& memory = *(uint8_t*) (&*buffer + index);
				memory = *(uint8_t*) (&*buffer + index);
				memory ^= (uint8_t) crypt_key_gen();
				index++;
			}
		}

		/* write the license header */
		memcpy(&*buffer, &license_header, sizeof(license_header));
		return std::string((char*) &*buffer, buffer_size);
	}

	bool License::private_data_editable() const {
		return this->private_data->private_key_calculable(this->_hierarchy.size() - 1);
	}

	bool License::write_private_data(const LicensePrivateWriteOptions& write_options) {
		if(this->_hierarchy.empty()) return false;

		uint8_t private_key[64]; //ed25519_sign requires 64 bytes (may it expects a public key in front?)
		if(!this->private_data->calculate_private_key(private_key, this->_hierarchy.size() - 1))
			return false;
		memcpy(private_key + 32, private_key, 32);

		auto public_key = this->generate_public_key(public_root_key.data());

		size_t length = 0, offset = 0;
		if(!this->private_data->write(nullptr, length, 65536, write_options))
			return false;

		if(this->private_buffer)
			::free(this->private_buffer);
		this->private_buffer_length = length;
		this->private_buffer = (uint8_t*) malloc(length);
		if(!this->private_buffer) return false;

		if(!this->private_data->write(this->private_buffer, offset, length, write_options))
			return false;

		ed25519_sign(this->private_buffer_sign.data(), this->private_buffer, length, public_key.data(), private_key);
		return true;
	}

	std::array<uint8_t, 32> License::generate_public_key(const uint8_t* root_key, int length) const {
		uint8_t hash_buffer[64];

		ge_p3 parent_key{};
		ge_cached parent_cached{};

		/* import the main parent key */
		ge_frombytes_negate_vartime(&parent_key, root_key);

		/* undo the negate */
		fe_neg(parent_key.X, parent_key.X);
		fe_neg(parent_key.T, parent_key.T);

		for(const auto& entry : this->_hierarchy) {
			if(length-- == 0) continue;

			ge_p3 e_pub_key{};
			ge_frombytes_negate_vartime(&e_pub_key, entry->public_key().data());

			ge_p3_to_cached(&parent_cached, &parent_key);

			/* malloc could fail, but we ignore this for now */
			if(!entry->hash(hash_buffer)) /* */;

			/* import hash (convert to a valid coordinate) */
			memset(hash_buffer + 32, 0, 32); /* yes, we have to drop half of the SHA512 hash :( */
			hash_buffer[0]  &= 0xF8U;
			hash_buffer[31] &= 0x3FU;
			hash_buffer[31] |= 0x40U;
			sc_reduce(hash_buffer);

			/* import the clamp data */
			ge_p3 p3_clamp_mul_pKey{};
			ge_p2 p2_clamp_mul_pKey{};
			ge_scalarmult_vartime(&p2_clamp_mul_pKey, hash_buffer, &e_pub_key);
			ge_p2_to_p3(&p3_clamp_mul_pKey, &p2_clamp_mul_pKey);

			/* add parent with the clamp data */
			ge_p1p1 a{};
			ge_add(&a, &p3_clamp_mul_pKey, &parent_cached);

			/* convert stuff back */
			ge_p3 r2{};
			ge_p1p1_to_p3(&r2, &a);

			parent_key = r2;
		}

		std::array<uint8_t, 32> result{};
		ge_p3_tobytes((uint8_t*) result.data(), &parent_key);
		return result;
	}

	bool License::push_entry(const std::shared_ptr<const HierarchyEntry> &entry, size_t* index) {
		assert(this->private_data);

		auto idx = this->_hierarchy.size();
		if(idx > 0 && !this->private_data->private_key_calculable(idx - 1))
			return false;

		if(index)
			*index = idx;
		this->_hierarchy.push_back(entry);
		return true;
	}

	typedef duration<int64_t, ratio<hours::period::num * 24, hours::period::den>>  days;
	typedef duration<int64_t, ratio<days::period::num * 365, days::period::den>>  years;

	bool License::hierarchy_timestamps_valid() {
		system_clock::time_point time_begin{};
		system_clock::time_point time_end = system_clock::time_point{} + years{5000};

		for(const auto& entry : this->_hierarchy) {
			auto end = entry->end_timestamp();
			auto begin = entry->begin_timestamp();
			if(begin < time_begin)
				return false;
			if(end > time_end)
				return false;

			time_begin = begin;
			if(end.time_since_epoch().count() != 0) time_end = end;
		}
		return true;
	}

	void License::_register_raw_private_key(size_t index, uint8_t *data) {
		assert(this->private_data);
		this->private_data->register_raw_private_key(index, data);
	}

	bool License::generate_keypair(uint8_t *prv, uint8_t *pbl) {
		std::random_device rd;
		std::uniform_int_distribution<uint8_t> d;

		uint8_t root_seed[64];
		for(auto& e : root_seed)
			e = d(rd);

		ed25519_create_keypair(pbl, prv, root_seed);
		return true;
	}

	std::shared_ptr<LicensePrivate> LicensePrivate::create(const std::shared_ptr<license::v2::License> &handle, int key_index, const uint8_t *key) {
		auto result = shared_ptr<LicensePrivate>(new LicensePrivate{});
		result->_handle = handle;
		result->precalculated_private_key_index = key_index;
		if(key) {
			memcpy(result->precalculated_private_key.data(), key, result->precalculated_private_key.size());
		} else {
			assert(key_index < -1);
		}
		return result;
	}

	std::shared_ptr<LicensePrivate> LicensePrivate::read(const std::shared_ptr<License>& handle, uint8_t version, const uint8_t *buffer, size_t length, uint8_t& error) {
		if(version != 2) {
			error = 2; /* invalid version */
			return nullptr;
		}

		auto result = LicensePrivate::create(handle, -2, nullptr);

		size_t offset = 0;
		if((offset + 1) > length)
			return nullptr;

		/* read the precalculated private key */
		if(*(buffer + offset++)) {
			if((offset + 1 + result->precalculated_private_key.size()) > length)
				return nullptr;

			result->precalculated_private_key_index = *(buffer + offset++);
			memcpy(result->precalculated_private_key.data(), buffer + offset, result->precalculated_private_key.size());
			offset += result->precalculated_private_key.size();
		}

		/* read raw private keys */
		{
			if((offset + 1) > length)
				return nullptr;

			auto private_key_count = *(buffer + offset++);

			if((offset + private_key_count * 33) > length)
				return nullptr;
			while(private_key_count-- > 0) {
				auto index = *(buffer + offset++);
				result->register_raw_private_key(index, buffer + offset);
				offset += 32;
			}
		}

		/* read the metadata */
		{
			if((offset + 4) > length)
				return nullptr;

			auto meta_data_length = *(uint32_t*) (buffer + offset);
			offset += 4;

			while(meta_data_length-- > 0) {
				if((offset + 3) > length)
					return nullptr;
				auto key_length = *(buffer + offset++);
				auto value_length = *(uint16_t*) (buffer + offset);
				offset += 2;

				if((offset + key_length + value_length) > length)
					return nullptr;

				result->meta_data[{(char*) buffer + offset, key_length}] = {(char*) buffer + offset + key_length, value_length};
				offset += key_length;
				offset += value_length;
			}
		}
		return result;
	}

	bool LicensePrivate::write(uint8_t *buffer, size_t &offset, size_t length, const LicensePrivateWriteOptions& options) {
		if(options.precalculated_key_index < -1) {
			if(buffer) {
				if((offset + 2) > length) return false;
				*(buffer + offset++) = 0; /* no precalculated private key */
				*(buffer + offset++) = 0; /* no raw private keys */
			} else {
				offset += 2;
			}
		} else {
			auto index = options.precalculated_key_index == -1 ? this->precalculated_private_key_index : options.precalculated_key_index;
			if(index < 0) return false; /* we will NEVER write the root key */

			if(buffer) {
				if((offset + 2 + 32) > length) return false;
				*(buffer + offset++) = 1;
				{
					*(buffer + offset++) = index;

					if(!this->calculate_private_key(buffer + offset, index))
						return false;
					offset += 32;
				}

				if((offset + 1) > length) return false;
				auto& private_key_count = *(buffer + offset++);
				private_key_count = 0;
				for(auto& [key_index, key] : this->private_keys) {
					if(key_index <= index)
						continue;

					if((offset + 1 + key.size()) > length) return false;
					*(buffer + offset++) = key_index;
					memcpy(buffer + offset, key.data(), key.size());

					private_key_count++;
					offset += key.size();
				}
			} else {
				/* private precalc key */
				offset += 2 + 32;

				/* raw keys */
				offset += 1;
				for(auto& [key_index, key] : this->private_keys) {
					if(key_index <= index)
						continue;

					offset += 1 + key.size();
				}
			}
		}

		if(buffer) {
			if((offset + 4) > length) return false;
			*(uint32_t*) (buffer + offset) = this->meta_data.size();
			offset += 4;

			for(auto& [key, value] : this->meta_data) {
				if((offset + 3 + key.length() + value.length()) > length) return false;

				*(buffer + offset++) = key.length();
				*(uint16_t*)(buffer + offset) = value.length();
				offset += 2;

				memcpy(buffer + offset, key.data(), key.length());
				offset += key.length();

				memcpy(buffer + offset, value.data(), value.length());
				offset += value.length();
			}
		} else {
			offset += 4;
			for(auto& [key, value] : this->meta_data)
				offset += 3 + key.length() + value.length();
		}

		return true;
	}

	bool LicensePrivate::private_key_chain_valid() {
		auto handle = this->_handle.lock();
		if(!handle) return false;

		auto hierarchy = handle->hierarchy();

		auto base_index = this->precalculated_private_key_index;
		if(base_index >= (int64_t) hierarchy.size()) return false;
		if(base_index < -1) return true; /* means we don't have a private key */

		while(base_index < hierarchy.size()) {
			if(!this->has_raw_private_key(base_index++))
				return false;
		}
		return true;
	}

	bool LicensePrivate::private_key_calculable(int index) const {
		auto handle = this->_handle.lock();
		if(!handle) return false;

		auto hierarchy = handle->hierarchy();
		if(index >= (int64_t) hierarchy.size()) return false;


		auto base_index = this->precalculated_private_key_index;
		if(base_index > index) return false;
		if(base_index < -1) return false;

		while(base_index < index) {
			base_index++;
			if(this->private_keys.count(base_index) < 1)
				return false; /* we're missing a private key here, how is this even possible? */
		}

		return true;
	}

	bool LicensePrivate::calculate_private_key(uint8_t *buffer, uint8_t index) const {
		auto handle = this->_handle.lock();
		if(!handle) return false;

		auto hierarchy = handle->hierarchy();
		if(index >= hierarchy.size()) return false;

		auto base_index = this->precalculated_private_key_index;
		if(base_index > index) return false;
		if(base_index < -1) return false;

		uint8_t hash_buffer[64];
		memcpy(buffer, this->precalculated_private_key.data(), this->precalculated_private_key.size());
		while(base_index < index) {
			base_index++;
			if(this->private_keys.count(base_index) < 1)
				return false; /* we're missing a private key here, how is this even possible? */

			if(!hierarchy[index]->hash(hash_buffer)) return false;

			/* import hash (convert to a valid coordinate) */
			memset(hash_buffer + 32, 0, 32); /* yes, we have to drop half of the SHA512 hash :( */
			hash_buffer[0]  &= 0xF8U;
			hash_buffer[31] &= 0x3FU;
			hash_buffer[31] |= 0x40U;
			sc_reduce(hash_buffer);

			sc_muladd(buffer, this->private_keys.at(base_index).data(), hash_buffer, buffer);
		}

		return true;
	}

	void LicensePrivate::register_raw_private_key(uint8_t index, const uint8_t *buffer) {
		auto& target = this->private_keys[index];
		memcpy(target.data(), buffer, target.size());
	}

	bool LicensePrivate::has_raw_private_key(uint8_t index) const {
		return this->private_keys.count(index) > 0;
	}

	HierarchyEntry::~HierarchyEntry() {
		this->allocate_read_body(0);
	}

	std::shared_ptr<const HierarchyEntry> HierarchyEntry::read(const uint8_t *buffer, size_t &offset, size_t length) {
		auto result = shared_ptr<HierarchyEntry>(new HierarchyEntry{});
		if((offset + 43) > length) return nullptr;

		result->_entry_type = *(buffer + offset);
		offset++;

		memcpy(result->_public_key.data(), buffer + offset, 32);
		offset += 32;

		memcpy(&result->_timestamp_begin, buffer + offset, 4);
		offset += 4;

		memcpy(&result->_timestamp_end, buffer + offset, 4);
		offset += 4;

		uint16_t body_length;
		memcpy(&body_length, buffer + offset, 2);
		offset += 2;

		if(body_length > length) return nullptr;
		if(!result->allocate_read_body(body_length)) return nullptr;

		if(body_length > 0) {
			result->allocate_read_body(body_length);
			if(!result->read_body) return nullptr;
			result->read_body_length = body_length;
			memcpy(result->read_body, buffer + offset, body_length);
			offset += body_length;
		}

		result->_hash_set = false;
		return result;
	}

	bool HierarchyEntry::write(uint8_t *buffer, size_t &offset, size_t length) const {
		if(buffer && (offset + 43 + this->read_body_length) > length) return false;

		if(buffer) *(buffer + offset) = this->_entry_type;
		offset++;

		if(buffer) memcpy(buffer + offset, this->_public_key.data(), 32);
		offset += 32;

		if(buffer) memcpy(buffer + offset, &this->_timestamp_begin, 4);
		offset += 4;

		if(buffer) memcpy(buffer + offset, &this->_timestamp_end, 4);
		offset += 4;

		if(buffer) memcpy(buffer + offset, &this->read_body_length, 2);
		offset += 2;

		if(this->read_body_length > 0) {
			if(buffer) memcpy(buffer + offset, this->read_body, this->read_body_length);
			offset += this->read_body_length;
		}

		return true;
	}

	bool HierarchyEntry::allocate_read_body(size_t size) {
		if(this->read_body) {
			::free(this->read_body);
			this->read_body = nullptr;
		}

		if(size > 0) {
			this->read_body = (uint8_t*) malloc(size);
			if(!this->read_body) return false;
		}
		return true;
	}

	bool HierarchyEntry::hash(uint8_t *target_buffer) const {
		if(this->_hash_set) {
			memcpy(target_buffer, this->_hash.data(), this->_hash.size());
			return true;
		}

		size_t length = 43 + this->read_body_length, offset = 0;
		auto buffer = unique_ptr<uint8_t, decltype(::free)*>((uint8_t*) malloc(length), ::free);
		if(!buffer) return false;
		if(this->write(&*buffer, offset, length)) {
			digest::sha512((char*) &*buffer, length, this->_hash.data());
			this->_hash_set = true;
		}

		return this->_hash_set ? this->hash(target_buffer) : false;
	}

	namespace hierarchy {
		std::string_view Intermediate::description() {
			if(this->_length == 0)
				return {};

			return std::string_view{(const char*) this->_memory + 1, (size_t) *this->_memory};
		}

		std::shared_ptr<const HierarchyEntry> Intermediate::create(const uint8_t *pub_key, const std::chrono::system_clock::time_point &begin, const std::chrono::system_clock::time_point & end, const std::string &description) {
			assert(description.size() < 256);
			auto buffer_length = description.size() + 1;

			uint8_t* buffer;
			auto result = BodyInterpreter::_create<Intermediate>(pub_key, begin, end, buffer_length, buffer);
			if(!result) return nullptr;

			memcpy(buffer + 1, description.data(), description.length());
			*buffer = (uint8_t) description.length();

			return result;
		}

		bool Server::has_username() {
			return *(this->_memory + 1) > 0;
		}

		std::string_view Server::contact_email() {
			return {(const char*) this->_memory + 2, *this->_memory};
		}

		std::string_view Server::username() {
			return {(const char*) this->_memory + 2 + *this->_memory, *(this->_memory + 1)};
		}

		std::shared_ptr<const HierarchyEntry> Server::create(const uint8_t *pub_key, const std::chrono::system_clock::time_point &begin, const std::chrono::system_clock::time_point & end, const std::string &email, const std::optional<std::string> &username) {
			assert(email.size() < 256);
			assert(!username.has_value() || username->size() < 256);
			auto buffer_length = 2 + email.size() + (username.has_value() ? username->length() : 0);

			uint8_t* buffer;
			auto result = BodyInterpreter::_create<Intermediate>(pub_key, begin, end, buffer_length, buffer);
			if(!result) return nullptr;

			memcpy(buffer + 2, email.data(), email.length());
			*buffer = (uint8_t) email.length();

			if(username.has_value())
				memcpy(buffer + 2 + email.length(), username->data(), username->length());
			*(buffer + 1) = (uint8_t) (username.has_value() ? username->length() : 0);

			return result;
		}

		std::shared_ptr<const HierarchyEntry> Ephemeral::create(const uint8_t *pub_key, const std::chrono::system_clock::time_point &begin, const std::chrono::system_clock::time_point & end) {
			uint8_t* buffer;
			return BodyInterpreter::_create<Ephemeral>(pub_key, begin, end, 0, buffer);
		}
	}
}