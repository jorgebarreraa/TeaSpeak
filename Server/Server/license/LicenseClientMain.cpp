#include <iostream>
#include <shared/include/license/license.h>
#include <shared/include/license/LicenseRequest.h>
#include <event2/thread.h>
#include "shared/include/license/client.h"

#include <random>
#include <ed25519/ed25519.h>
#include <misc/base64.h>

using namespace std;
using namespace std::chrono;
using namespace license;

/*
    struct LicenseInfo {
        LicenseType type;
	    std::string username;
	    std::string first_name;
        std::string last_name;
        std::string email;
        std::chrono::system_clock::time_point start;
	    std::chrono::system_clock::time_point end;
	    std::chrono::system_clock::time_point creation;

        inline bool isValid() { return (end.time_since_epoch().count() == 0 || std::chrono::system_clock::now() < this->end); }
    };
 */

std::array<uint8_t, 195> intermediate_key{0x02, 0x00, 0x2d, 0x6b, 0xe3, 0x4d, 0x3c, 0xbb, 0x19, 0x1e, 0x46, 0x25, 0x72, 0x22, 0xa3, 0x53, 0x6d, 0x2d, 0xc3, 0xd1, 0x2c, 0xc8, 0xea, 0xf2, 0xf8, 0xe5, 0xd5, 0x0f, 0x6c, 0x7f, 0xeb, 0x63, 0x8a, 0xc4, 0x37, 0xb2, 0x33, 0x5e, 0x06, 0x31, 0xcf, 0x2c, 0xc6, 0xbe, 0x5e, 0x9c, 0xf1, 0xe6, 0xb3, 0xc3, 0x69, 0xd7, 0xf0, 0x05, 0x90, 0x2f, 0x65, 0x03, 0x60, 0x12, 0xa0, 0x20, 0x83, 0xe2, 0x6b, 0x4f, 0x46, 0xab, 0xfd, 0xa6, 0x22, 0x0b, 0x11, 0x9a, 0x34, 0xfe, 0x7b, 0xa3, 0x1b, 0x12, 0xce, 0x30, 0xf7, 0x0a, 0x32, 0x26, 0x23, 0x10, 0xfe, 0x58, 0xb6, 0xc7, 0x5d, 0x3d, 0xc6, 0x14, 0xdc, 0x10, 0xa0, 0xc0, 0x78, 0x10, 0x45, 0x41, 0x36, 0x1a, 0x1c, 0x9f, 0x60, 0x25, 0xd9, 0x69, 0xc9, 0x0b, 0xb7, 0xb4, 0x64, 0xab, 0x6c, 0x22, 0xff, 0xaf, 0x86, 0x26, 0x94, 0xcc, 0x7f, 0x59, 0x52, 0xa3, 0x56, 0x7f, 0x3e, 0x86, 0x04, 0x4c, 0xe0, 0x0e, 0xb3, 0xb1, 0x23, 0x51, 0xf7, 0xf0, 0x14, 0x5d, 0xfd, 0x48, 0xfb, 0x16, 0xe6, 0xc6, 0xca, 0xf2, 0x8d, 0xc8, 0xce, 0xf1, 0x2b, 0x12, 0x9e, 0xd1, 0x7a, 0x80, 0x3a, 0x9b, 0x46, 0xe7, 0xca, 0x34, 0x04, 0xae, 0x3d, 0x12, 0xcd, 0x4a, 0xc6, 0xe1, 0xf1, 0xe4, 0xd8, 0xca, 0x68, 0x36, 0xd3, 0x94, 0x0d, 0xef, 0x93, 0x86, 0xb9, 0x3a, 0xa4, 0x10, 0x52};
int main(int ac, char** av){
	auto state = evthread_use_pthreads();
	assert(state == 0);
	string error;
	uint8_t errc = 0;

#if 0
	std::array<uint8_t, 32> prv_key{};
	auto license = v2::License::create({}, prv_key);
	assert(license->private_data_editable());

	license->push_entry<v2::hierarchy::Intermediate>(system_clock::now(), system_clock::now() + hours{24 * 365 * 1000}, "TeaSpeak (Test)");
	assert(license->write_private_data({0}));
	error = license->write(errc);
	assert(errc == 0);

	cout << "Intermediate: " << hex;
	for(size_t index = 0; index < error.length(); index++) {
		uint8_t e = error[index];
		cout << " 0x" << (e <= 0xF ? "0" : "") << (uint32_t) e << ",";
	}
	cout << endl;
	cout << "Intermediate: " << base64::encode(error) << endl;
#endif
#if 0
	auto license = v2::License::read(intermediate_key.data(), intermediate_key.size(), errc);

	license->push_entry<v2::hierarchy::Server>(system_clock::now(), system_clock::now() + hours{24 * 365 * 1000}, "TeaSpeak Official server", "contact@teaspeak.de");
	assert(license->write_private_data({0}));
	error = license->write(errc);
	assert(errc == 0);
	__asm__("nop");
	cout << "Errc: " << errc << endl;
	cout << "Write: " << base64::encode(error) << endl;
#endif
#if 0
	std::array<uint8_t, 32> private_key, public_key;

	std::random_device rd;
	std::uniform_int_distribution<uint8_t> d;

	uint8_t root_seed[64];
	for(auto& e : root_seed)
		e = d(rd);
	ed25519_create_keypair(public_key.data(), private_key.data(), root_seed);

	cout << "Key Pair generated:" << endl;
	cout << "Private Key:" << hex;
	for(auto& e : private_key)
		cout << " 0x" << (e <= 0xF ? "0" : "") << (uint32_t) e << ",";
	cout << endl;

	cout << "Public Key :" << hex;
	for(auto& e : public_key)
		cout << " 0x" << (e <= 0xF ? "0" : "") << (uint32_t) e << ",";
	cout << endl;

	return true;
#endif
#if 0
	srand(system_clock::now().time_since_epoch().count());
	cout << "Generating new license" << endl;

	std::string name = "WolverinDEV";
	auto raw_license = createLocalLicence(LicenseType::PREMIUM, system_clock::now() - chrono::hours((int) (24 * 30.5 * 3)), "WolverinDEV");
	auto license = readLocalLicence(raw_license, error);
	assert(license);



	sockaddr_in serv_addr{};
	serv_addr.sin_family = AF_INET;

	serv_addr.sin_addr.s_addr = ((in_addr*) gethostbyname("localhost")->h_addr)->s_addr;
	serv_addr.sin_port = htons(27786);


	/*
	 * struct LicenseRequestData {
		std::shared_ptr<License> license;

		int64_t speach_total;
		int64_t speach_dead;
		int64_t speach_online;
		int64_t speach_varianz;

		int64_t client_online;
		int64_t bots_online;
		int64_t queries_online;
		int64_t servers_online;
	};
	 */
	auto data = make_shared<LicenseRequestData>();
	data->license = license;
	data->info = make_shared<ServerInfo>();

    LicenceRequest request(data, serv_addr);
    try {
        cout << "Requesting license" << endl;
        auto info = request.requestInfo().waitAndGet(nullptr);
        if(!info) {
            cout << "Invalid result! Error: " << (request.exception() ? "yes => " + string(request.exception()->what()) : "no")  << endl;
            throw *request.exception();
        }
        cout << "Got result!" << endl;
        cout << "Valid: " << info->license_valid << endl;
        if(info->license) {
            cout << "License:" << endl;
            cout << "  Type: " << info->license->type << endl;
            cout << "  User name: " << info->license->username << endl;
            cout << "  First name: " << info->license->first_name << endl;
            cout << "  Last name: " << info->license->last_name << endl;
            cout << "  EMail: " << info->license->email << endl;
        } else cout << "License: none";
    } catch (const std::exception& ex){
        cerr << "Could not load info after throwing: " << endl << ex.what() << endl;
    }
#endif
#if 1
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;

    serv_addr.sin_addr.s_addr = ((in_addr*) gethostbyname("localhost")->h_addr)->s_addr;
    serv_addr.sin_port = htons(27786);

    client::LicenseServerClient client{serv_addr, 3};
    client.callback_connected = [&]{
        std::cout << "Connected" << std::endl;
        client.disconnect("client closed", std::chrono::system_clock::now() + std::chrono::seconds{5});
    };
    client.callback_message = [&](auto type, const void* buffer, size_t length) {
        std::cout << "Received an message" << std::endl;
    };
    client.callback_disconnected = [&](bool expected, const std::string& reason) {
        std::cout << "Received disconnect (expected: " << expected << "): " << reason << std::endl;
    };

    if(!client.start_connection(error)) {
        std::cout << "Failed to start connection" << std::endl;
        return 0;
    }

    std::this_thread::sleep_for(std::chrono::seconds{2});
    client.disconnect("client closed", std::chrono::system_clock::now() + std::chrono::seconds{5});
    std::cout << "Disconnect result: " << client.await_disconnect() << std::endl;
#endif
	return 0;
}