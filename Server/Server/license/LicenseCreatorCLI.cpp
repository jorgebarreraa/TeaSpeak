#include <iostream>
#include <algorithm>
#include <vector>
#include <event2/thread.h>
#include <misc/base64.h>

#include "manager/ServerConnection.h"

using namespace std;
using namespace license;
using namespace license::manager;
using namespace std::chrono;

std::string url_decode(const std::string& source) {
	std::string ret;
	char ch;
	size_t i;
	int ii;
	for (i=0; i < source.length(); i++) {
		if ((int) source[i] == 37) {
			sscanf(source.substr(i + 1, 2).c_str(), "%x", &ii);
			ch = static_cast<char>(ii);
			ret += ch;
			i = i + 2;
		} else {
			ret += source[i];
		}
	}
	return ret;
}

class CLIParser {
	public:
		CLIParser(int &argc, char **argv) {
			for (int i = 1; i < argc; ++i)
				this->tokens.emplace_back(argv[i]);
		}

		/// @author iain
		const std::string& get_option(const std::vector<std::string> &options) const {
			for(const auto& option : options) {
				std::vector<std::string>::const_iterator itr;
				itr = std::find(this->tokens.begin(), this->tokens.end(), option);
				if (itr != this->tokens.end() && ++itr != this->tokens.end())
					return *itr;
			}

			static const std::string empty_string;
			return empty_string;
		}

		/// @author iain
		bool option_exists(const std::vector<std::string> &options, bool allow_empty = true) const {
			for(const auto& option : options) {
				auto index = std::find(this->tokens.begin(), this->tokens.end(), option);
				if(index != this->tokens.end())
					if(allow_empty || !index->empty())
						return true;
			}
			return false;
		}

	private:
		std::vector<std::string> tokens;
};

#define _str(x) #x

#define REQ_CMD(variable, message, skey, lkey, ...)                                  \
if(!options.option_exists({skey, lkey, #__VA_ARGS__}, false)) {                      \
    cerr << message << " (" << _str(skey) << " or " << _str(lkey) << ")" << endl;    \
    return 1;                                                                        \
}                                                                                    \
auto variable = url_decode(options.get_option({skey, lkey, #__VA_ARGS__}))           \

#define NO_OPEN_SSL
#include <misc/digest.h>

int main(int argc, char **argv) {
	CLIParser options(argc, argv);

	REQ_CMD(user, "missing user", "-u", "--user");
	REQ_CMD(first_name, "missing first name", "-n", "--first_name");
	REQ_CMD(last_name, "missing last name", "-s", "--last_name");
	REQ_CMD(begin, "missing begin timestamp", "-b", "--begin");
	REQ_CMD(end, "missing end timestamp", "-e", "--end");
	REQ_CMD(email, "missing email", "-m", "--email");
	REQ_CMD(license_type, "missing license_type", "-l", "--license-type");

	REQ_CMD(auth_user, "missing authentication user", "-au", "--auth-user");
	REQ_CMD(auth_pass, "missing authentication user", "-ap", "--auth-pass");

	REQ_CMD(server_host, "missing server host", "-h", "--server-host");
    REQ_CMD(server_port, "missing server port", "-p", "--server-port");

    REQ_CMD(old_key, "missing old license key", "-ol", "--old-license");

	auto state = evthread_use_pthreads();
	if(state != 0) {
		cerr << "failed to setup pthreads" << endl;
		return 1;
	}

	std::string error{};
	ServerConnection connection;
	connection.verbose = false;
	try {
		auto future = connection.connect(server_host, (uint16_t) stol(server_port));
		if(!future.waitAndGet(false, system_clock::now() + seconds(5))) {
			cerr << "failed to connect (" << future.errorMegssage() << ")" << endl;
			return 1;
		}
	} catch(std::exception& ex) {
		cerr << "invalid port" << endl;
		return 1;
	}

	{
		auto future = connection.login(auth_user, auth_pass);
		if(!future.waitAndGet(false, system_clock::now() + seconds(5))) {
			cerr << "failed to authenticate (" << future.errorMegssage() << ")" << endl;
			return 1;
		}
	};

    std::shared_ptr<License> old_license{nullptr};
	if(!old_key.empty() && old_key != "none") {
	    old_license = license::readLocalLicence(old_key, error);
	    if(!old_license) {
	        cerr << "failed to decode old license: " << error << endl;
	        return 1;
	    }
	}

	try {
		system_clock::time_point timestamp_begin = system_clock::time_point() + seconds(stoll(begin));
		system_clock::time_point timestamp_end = system_clock::time_point() + seconds(stoll(end));
		auto future = connection.registerLicense(first_name, last_name, user, email, (LicenseType) stoll(license_type), timestamp_end, timestamp_begin, old_license ? old_license->key() : "none");
		auto result = future.waitAndGet({nullptr, nullptr}, system_clock::now() + seconds(5));
		if(!result.first || !result.second) {
			cerr << "failed to create license! (" << future.errorMegssage() << ")" << endl;
			return 1;
		}

		auto license_key = license::exportLocalLicense(result.first);
		cout << license_key << endl;
	} catch(std::exception& ex) {
		cerr << "invalid timestamps or license type" << endl;
		return 1;
	}

	return 0;
}