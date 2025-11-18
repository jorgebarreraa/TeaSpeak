#include <iostream>
#include <event2/thread.h>
#include <cstring>
#include <variant>
#include <random>
#include <deque>
#include <cassert>

#include "../src/response.h"
#include "../src/resolver.h"
#include "../utils.h"

using namespace tc::dns;
using namespace std;

namespace parser = response::rrparser;

int main() {
#ifdef WIN32
    {
        WSADATA wsaData;
        auto error = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (error != 0) {
            wprintf(L"WSAStartup failed with %d\n", error);
            return error;
        }
    }

    evthread_use_windows_threads();
#else
	evthread_use_pthreads();
#endif
	//evthread_enable_lock_debugging();

	std::string error{};
	Resolver resolver{};

	if(!resolver.initialize(error, true, true)) {
		cerr << "failed to initialize resolver: " << error << endl;
		return 1;
	}

	auto begin = chrono::system_clock::now();
	for(int i = 0; i < 1; i++) {
		cr(resolver, {
				"ttesting.ts3index.com", //alex.ts3clan.pp.ua
				9987
		}, [begin](bool success, auto data) {
			auto end = chrono::system_clock::now();
			cout << "Success: " << success << " Time: " << chrono::ceil<chrono::milliseconds>(end - begin).count() << "ms" << endl;
			if(success) {
				auto address = std::get<ServerAddress>(data);
				cout << "Target: " << address.host << ":" << address.port << endl;
			} else {
				cout << "Message: " << std::get<std::string>(data) << endl;
			}
		});
	}

	this_thread::sleep_for(chrono::seconds{5});
	cout << "Timeout" << endl;
	resolver.finalize();
    cout << "Exit" << endl;

	return 0;
}