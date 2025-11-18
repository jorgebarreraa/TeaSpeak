#include <iostream>
#include <openssl/bio.h>
#include <server/LicenseServer.h>
#include <log/LogUtils.h>
#include <CXXTerminal/Terminal.h>
#include <sql/mysql/MySQL.h>
#include "server/WebAPI.h"
#include "server/StatisticManager.h"
#include <event2/thread.h>
#include "server/UserManager.h"

#include <misc/digest.h>
#include <misc/hex.h>

using namespace std;
using namespace std::chrono;
using namespace license;

/*
 * Requests/license: SELECT `tmp`.`keyId`, `tmp`.`key`, `tmp`.`ip`, `tmp`.`count`, `license_info`.`username`, `tmp`.`type` FROM (SELECT DISTINCT `license_request`.`keyId`, `key`, `license_request`.`ip`, COUNT(`license_request`.`keyId`) AS `count`, `license`.`type` FROM `license_request` INNER JOIN `license` ON `license`.`keyId` = `license_request`.`keyId` GROUP BY (`license_request`.`ip`)) AS `tmp` INNER JOIN `license_info` ON `license_info`.`keyId` = `tmp`.`keyId`
 * Different IP's: SELECT `tmp`.`keyId`, `license_info`.`username`, COUNT(`ip`) FROM (SELECT DISTINCT `ip`, `keyId` FROM `license_request`) AS `tmp` LEFT JOIN `license_info` ON `license_info`.`keyId` = `tmp`.`keyId` GROUP BY (`tmp`.`keyId`) ORDER BY COUNT(`ip`) DESC
 *
 * Requests (license) / ip: SELECT DISTINCT `ip`, COUNT(`ip`) FROM `license_request` WHERE `keyId` = ? GROUP BY `ip`
 *
 * SELECT DISTINCT(`ip`), `keyId` FROM `license_request` WHERE `timestamp` > (UNIX_TIMESTAMP() - 2 * 60 * 60) * 1000
 * //462
 *
 * SELECT DISTINCT(`ip`), `license_request`.`keyId`, `license_info`.`username` FROM `license_request` LEFT JOIN `license_info` ON `license_info`.`keyId` = `license_request`.`keyId` WHERE `timestamp` > (UNIX_TIMESTAMP() - 2 * 60 * 60) * 1000
 * License too many request looking: SELECT `keyId`, `username`, COUNT(`ip`) FROM `unique_license_requests` GROUP BY `keyId` ORDER BY COUNT(`ip`) DESC
 *
 *
 *
 * Online clients:   SELECT SUM(`clients`) FROM (SELECT DISTINCT(`ip`), `clients` FROM `history_online` WHERE `timestamp` > (UNIX_TIMESTAMP() - 60 * 60 * 2) * 1000) AS `a`
 * Online bots:      SELECT SUM(`clients`) FROM (SELECT DISTINCT(`ip`), `clients` FROM `history_online` WHERE `timestamp` > (UNIX_TIMESTAMP() - 60 * 60 * 2) * 1000) AS `a`
 * Online VS Server: SELECT SUM(`music`) FROM (SELECT DISTINCT(`ip`), `music` FROM `history_online` WHERE `timestamp` > (UNIX_TIMESTAMP() - 60 * 60 * 2) * 1000) AS `a`
 *
 * Empty instances: curl -ik "https://stats.teaspeak.de:27788?type=request&request_type=general" -X GET 2>&1 | grep "data:"
 */
//SELECT SUM(`clients`) FROM `history_online` WHERE `keyId` = 701 OR `keyId` = 795 OR `keyId` = 792 OR `keyId` = 582 OR `keyId` = 753 OR `keyId` = 764 OR `keyId` = 761 OR `keyId` = 796 WHERE `timestamp` > (UNIX_TIMESTAMP() - 2.1 * 60 * 60) * 1000

/*
 * Extra users:
 * - ServerSponsoring
 * - Davide550
 * - xDeyego?
 * - latters
 * - Pamonha
 * - vova3639 (5 licenses)
 */

bool handle_command(string& line);

shared_ptr<server::database::DatabaseHandler> license_manager;
shared_ptr<stats::StatisticManager> statistic_manager;
shared_ptr<ts::ssl::SSLManager> ssl_manager;
shared_ptr<license::web::WebStatistics> web_server;
shared_ptr<LicenseServer> license_server;
shared_ptr<UserManager> user_manager;

inline std::shared_ptr<WebCertificate> load_web_certificate() {
	std::string certificate_file{"web_certificate.txt"}, certificate{};
	std::string key_file{"web_key.txt"}, key{};
	std::string error{};

	auto context = ssl_manager->initializeContext("web_shared_cert", key_file, certificate_file, error);
	if(!context) {
		logError(0, "Failed to load web certificated: {}", error);
		return nullptr;
	}


	std::shared_ptr<BIO> bio{nullptr};
	const uint8_t* mem_ptr{nullptr};
	size_t length{0};

	{
		bio = shared_ptr<BIO>(BIO_new(BIO_s_mem()), ::BIO_free);
		if(PEM_write_bio_PrivateKey(&*bio, &*context->privateKey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
			logError(0, "Failed to export certificate");
			return nullptr;
		}

#ifdef CRYPTO_BORINGSSL
        if(!BIO_mem_contents(&*bio_private_key, &mem_ptr, &length))  {
            logError(0, "Failed to receive memptr to private key");
            return nullptr;
        }
#else
        BUF_MEM* memory{nullptr};
        if(!BIO_get_mem_ptr(&*bio, &memory) || !memory) {
            logError(0, "Failed to receive memptr to private key");
            return nullptr;
        }

        mem_ptr = (uint8_t*) memory->data;
        length = memory->length;
#endif
		key.resize(length);
		memcpy(key.data(), mem_ptr, length);
	}

	{
		bio = shared_ptr<BIO>(BIO_new(BIO_s_mem()), ::BIO_free);
		if(PEM_write_bio_X509(&*bio, &*context->certificate) != 1) {
			logError(0, "Failed to export certificate");
			return nullptr;
		}
#ifdef CRYPTO_BORINGSSL
        if(!BIO_mem_contents(&*bio_private_key, &mem_ptr, &length)) {
            logError(0, "Failed to receive memptr to public key");
            return nullptr;
        }
#else
        BUF_MEM* memory{nullptr};
        if(!BIO_get_mem_ptr(&*bio, &memory) || !memory) {
            logError(0, "Failed to receive memptr to public key");
            return nullptr;
        }

        mem_ptr = (uint8_t*) memory->data;
        length = memory->length;
#endif
		certificate.resize(length);
		memcpy(certificate.data(), mem_ptr, length);
	}

	auto response = std::make_shared<WebCertificate>();
	response->key = key;
	response->certificate = certificate;
	response->revision = digest::sha512(response->key + response->certificate);
	logMessage(0, "Web certificate revision: {}", hex::hex(response->revision));
	return response;
}

int main(int argc, char** argv) {
    if(argc < 2) {
        cerr << "Invalid arguments! Need MySQL connection" << endl;
        return 0;
    }

	evthread_use_pthreads();
    srand(system_clock::now().time_since_epoch().count());

    //terminal::install();
    //if(!terminal::active()){ cerr << "could not setup terminal!" << endl; return -1; }

    auto config = std::make_shared<logger::LoggerConfig>();
    config->vs_group_size = 0;
    config->logfileLevel = spdlog::level::trace;
    config->terminalLevel = spdlog::level::trace;
    config->logPath = "logs/log_${time}(%Y-%m-%d_%H:%M:%S).log";
    config->sync = !terminal::active();
    logger::setup(config);

    string error;
    sql::SqlManager* database = new sql::mysql::MySQLManager();
    bool db_connected = true;
	((sql::mysql::MySQLManager*) database)->listener_disconnected = [&](bool wanted){
		if(wanted) return;
		logCritical(LOG_GENERAL, "Lost connection to MySQL server!");
		logCritical(LOG_GENERAL, "Stopping server!");
		db_connected = false;
	};

    //mysql://localhost:3306/license?userName=root&password=markus
    logMessage(LOG_GENERAL, "Connecting to {}", argv[1]);
    auto connect_result = database->connect(string(argv[1]));
    if(!connect_result) {
        logError(LOG_GENERAL, "Could not connect to mysql server! (" + connect_result.fmtStr() + ")");
        return 0;
    }
#if false
    sql::command(database, "INSERT INTO license (`key`, type, deleted, issuer) VALUES ('0020', 1, 1, 'Test'); ").execute();
    cout << sql::command(database, "SELECT LAST_INSERT_ID();").query([](void*, int length, string* values, string* names) {
        for(int i = 0; i < length; i++)
            cout << names[i] << " -> " << values[i] << endl;
        return 0;
    }, (void*) nullptr) << endl;
#endif

	license_manager = make_shared<server::database::DatabaseHandler>(database);
    if(!license_manager->setup(error)) {
        logError(LOG_GENERAL, "Could not start license manager! (" +error + ")");
        return 0;
    }


    statistic_manager = make_shared<stats::StatisticManager>(license_manager);

#if false
    /*
	{
		auto _now = system_clock::now();
		auto statistics = license_manager->list_statistics_user(_now - hours(24) * 32 * 4, _now, duration_cast<milliseconds>(hours(2)));
		cout << "Date,Instances,Servers,Clients,Web Clients,Queries,Music Bots" << endl;
		for(const auto& entry : statistics) {
			auto time = system_clock::to_time_t(entry->timestamp);
			tm* localtm = localtime(&time);
			string string_time = asctime(localtm);
			string_time = string_time.substr(0, string_time.length() - 1);
			//cout << "[" << string_time << "] Users online: " << entry->clients_online << " | Instances: " << entry->instance_online << endl;
			cout << string_time << "," <<  entry->instance_online << "," << entry->servers_online << "," << entry->clients_online << "," << entry->web_clients_online << "," << entry->queries_online << "," << entry->bots_online << endl;
		}
 	}
	return 0;
	*/
#endif

#if false
    /*
    {
    	ofstream _file_out("version_history.txt");

		auto _now = system_clock::now();
		cout << "Getting statistics" << endl;
		auto statistics = license_manager->list_statistics_version(_now - hours(24) * 32 * 12, _now, duration_cast<milliseconds>(hours(1)));
	    cout << "Grouping statistics" << endl;
		std::deque<std::string> versions;
		const auto version_name = [](const std::string& key) {
			auto space = key.find(' ');
			return key.substr(0, space);
		};

		for(const auto& entry : statistics) {
			for(const auto& version : entry->versions) {
				const auto name = version_name(version.first);
				if(name.empty()) {
					continue;
				}
				auto it = find(versions.begin(), versions.end(), name);
				if(it == versions.end())
					versions.push_back(name);
			}
		}
	    cout << "Sorting statistics" << endl;
		sort(versions.begin(), versions.end(), [](const std::string& a, const std::string& b) {
			const auto index_a = a.find_last_of('.');
			const auto index_b = b.find_last_of('.');
			const auto length_a = a.find('-', index_a) - index_a - 1;
			const auto length_b = b.find('-', index_b) - index_b - 1;

			const auto patch_a = stoll(a.substr(index_a + 1, length_a));
			const auto patch_b = stoll(b.substr(index_b + 1, length_b));
			return patch_a > patch_b;
		});

	    cout << "Writing statistics" << endl;
	    _file_out << "Date";
		for(auto & version : versions)
			_file_out << "," << version;
	    _file_out << endl;

		for(const auto& entry : statistics) {
			auto time = system_clock::to_time_t(entry->timestamp);
			tm* localtm = localtime(&time);
			string string_time = asctime(localtm);
			string_time = string_time.substr(0, string_time.length() - 1);

			map<string, int> version_count;
			for(const auto& version : entry->versions) {
				const auto name = version_name(version.first);
				version_count[name] += version.second;
			}

			_file_out << string_time;
			for(const auto& name : versions) {
				_file_out << "," << version_count[name];
			}
			_file_out << endl;
		}s
	    cout << "Done statistics" << endl;
		_file_out.flush();
    }
	return 0;
     */
#endif

	ssl_manager = make_shared<ts::ssl::SSLManager>();
	{
		string key_file = "certificates/web_stats_prv.pem";
		string cert_file = "certificates/web_stats_crt.pem";
		if(!ssl_manager->initializeContext("web_stats", key_file, cert_file, error, false, make_shared<ts::ssl::SSLGenerator>(ts::ssl::SSLGenerator{
				.subjects = {},
				.issues = {{"O", "TeaSpeak"}, {"OU", "License server"}, {"creator", "WolverinDEV"}}
		}))) {
			logCritical(LOG_LICENSE_WEB, "Failed to initialize ssl certificate! Stopping server.");
		}
	}

	{
		web_server = make_shared<license::web::WebStatistics>(license_manager, statistic_manager);
		logMessage(LOG_GENERAL, "Starting web server on [:::]:27788");
		if(!web_server->start(error, 27788, ssl_manager->getContext("web_stats"))) {
			logError(LOG_GENERAL, "Failed to start web statistics server!");
			return 0;
		}
	}

	{
		user_manager = make_shared<UserManager>(database);
	}
	{
		logMessage(LOG_GENERAL, "Starting license server on [:::]:27786");
		struct sockaddr_in listen_addr{};
		memset(&listen_addr, 0, sizeof(listen_addr));
		listen_addr.sin_family = AF_INET;
		listen_addr.sin_addr.s_addr = INADDR_ANY;
		listen_addr.sin_port = htons(27786);

		license_server = make_shared<LicenseServer>(listen_addr, license_manager, statistic_manager, web_server, user_manager);
		license_server->web_certificate = load_web_certificate();
        license_server->start();
	}

	while(db_connected && web_server->running() && license_server->isRunning()) {
	    if(!terminal::instance()) {
	        std::this_thread::sleep_for(std::chrono::seconds{10});
            continue;
	    }
		auto line = terminal::instance()->readLine("§a> §f");
		if(line.empty()){
			usleep(500);
			continue;
		}
		if(!handle_command(line))
			break;
	}

    terminal::instance()->writeMessage("§aStopping server...");
	web_server->stop();
    license_server->stop();
    if(database) database->disconnect();

    logger::uninstall();
    terminal::uninstall();
    return 0;
}

bool handle_command(string& line) {
	if(line == "end" || line == "stop") return false; //Exit loop

	if(line == "info web") {
		logMessage(LOG_LICENSE_WEB, "Currently online clients:");
		auto clients = web_server->get_clients();
		for(const auto& client : clients)
			logMessage(LOG_LICENSE_WEB, "  - {}", client->client_prefix());
		logMessage(LOG_LICENSE_WEB, " {} clients are currently connected!", clients.size());
		return true;
	}
	logError(LOG_GENERAL, "Invalid command: " + line);
	return true;
}