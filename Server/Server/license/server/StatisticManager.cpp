//
// Created by wolverindev on 04.09.18.
//

#include <sql/SqlQuery.h>
#include <misc/std_unique_ptr.h>

#include <utility>
#include "StatisticManager.h"
#include "DatabaseHandler.h"

using namespace std;
using namespace std::chrono;
using namespace license;
using namespace license::server;
using namespace license::stats;

std::chrono::milliseconds HistoryStatistics::time_period(license::stats::HistoryStatistics::HistoryType type) {
	switch (type) {
		case HistoryType::LAST_DAY:
		case HistoryType::DAY_YESTERDAY:
		case HistoryType::DAY_7DAYS_AGO:
			return minutes(15);
		case HistoryType::LAST_WEEK:
			return hours(1);
		case HistoryType::LAST_MONTH:
		case HistoryType::LAST_HALF_YEAR:
		default:
			return hours(2);
	}
}

std::chrono::milliseconds HistoryStatistics::cache_timeout(license::stats::HistoryStatistics::HistoryType type) {
	switch (type) {
		case HistoryType::LAST_DAY:
		case HistoryType::DAY_YESTERDAY:
		case HistoryType::DAY_7DAYS_AGO:
			return minutes(15);
		case HistoryType::LAST_WEEK:
			return hours(1);
		case HistoryType::LAST_MONTH:
			return hours(2);

		case HistoryType::LAST_HALF_YEAR:
		default:
			return hours(8);
	}
}

std::chrono::milliseconds HistoryStatistics::type_duration(license::stats::HistoryStatistics::HistoryType type) {
	switch (type) {
		case HistoryType::LAST_DAY:
		case HistoryType::DAY_YESTERDAY:
		case HistoryType::DAY_7DAYS_AGO:
			return hours(24);
		case HistoryType::LAST_WEEK:
			return hours(24) * 7;
		case HistoryType::LAST_MONTH:
			return hours(24) * 32;
		case HistoryType::LAST_HALF_YEAR:
			return hours(24) * 32 * 6;
		default:
			return hours(24);
	}
}

system_clock::time_point HistoryStatistics::align_type(license::stats::HistoryStatistics::HistoryType type, const std::chrono::system_clock::time_point &tp) {
	switch (type) {
		case HistoryType::LAST_DAY:
		case HistoryType::DAY_YESTERDAY:
		case HistoryType::DAY_7DAYS_AGO:
			return system_clock::time_point() + minutes(duration_cast<minutes>(tp.time_since_epoch()).count());
		case HistoryType::LAST_WEEK:
		case HistoryType::LAST_MONTH:
		case HistoryType::LAST_HALF_YEAR:
		default:
			return system_clock::time_point() + hours(duration_cast<hours>(tp.time_since_epoch()).count());
	}
}

StatisticManager::StatisticManager(std::shared_ptr<license::server::database::DatabaseHandler> manager) : license_manager{std::move(manager)} {}
StatisticManager::~StatisticManager() = default;

struct GeneralStatisticEntry {
	std::chrono::system_clock::time_point age;
	string unique_id{""};
	uint64_t key_id{0};
	uint64_t servers{0};
	uint64_t clients{0};
	uint64_t bots{0};
};

void StatisticManager::reset_cache_general() {
	lock_guard<recursive_mutex> lock(this->_general_statistics_lock);
	this->_general_statistics = nullptr;
}

void parse_general_entry(std::deque<std::unique_ptr<GeneralStatisticEntry>>& entries, bool unique, int length, string* values, string* names) {
	auto entry = make_unique<GeneralStatisticEntry>();
	for(int index = 0; index < length; index++) {
		if(names[index] == "keyId") {
			entry->key_id = stoull(values[index]);
		} else if(names[index] == "timestamp") {
			entry->age = system_clock::time_point() + milliseconds(stoll(values[index]));
		} else if(names[index] == "server") {
			entry->servers = stoull(values[index]);
		} else if(names[index] == "clients") {
			entry->clients = stoull(values[index]);
		} else if(names[index] == "music") {
			entry->bots = stoull(values[index]);
		} else if(names[index] == "unique_id")
			entry->unique_id = values[index];
	}

	if(unique) {
		for(auto& e : entries) {
			if(e->key_id == entry->key_id && e->unique_id == entry->unique_id) {
				if(e->age < entry->age) {
					entries.erase(find(entries.begin(), entries.end(), e));
					break;
				} else {
					return;
				}
			}
		}
	}
	entries.push_back(std::move(entry));
}

std::shared_ptr<GeneralStatistics> StatisticManager::general_statistics() {
	unique_lock<recursive_mutex> lock(this->_general_statistics_lock);
	if(this->_general_statistics && system_clock::now() < this->_general_statistics->age + seconds(300)) return this->_general_statistics;

	lock.unlock();
	unique_lock create_lock(this->_general_statistics_generate_lock);
	lock.lock();
	if(this->_general_statistics && system_clock::now() < this->_general_statistics->age + seconds(300)) return this->_general_statistics;
	lock.unlock();

	deque<unique_ptr<GeneralStatisticEntry>> entries;

	//TODO: Calculate web clients!
	auto result = sql::command(this->license_manager->sql(), "SELECT `keyId`, `unique_id`, `timestamp`,`server`,`clients`,`music` FROM `history_online` WHERE `timestamp` > :time ORDER BY `timestamp` ASC",
				variable{":time", duration_cast<milliseconds>(system_clock::now().time_since_epoch() - hours(2) - minutes(10)).count()}) //10min as buffer
			.query(std::function<decltype(parse_general_entry)>{parse_general_entry}, entries, true);

	auto stats = make_shared<GeneralStatistics>();
	for(auto& entry : entries) {
		stats->bots += entry->bots;
		stats->clients += entry->clients;
		stats->servers += entry->servers;
        stats->empty_instances += entry->clients == 0;
		stats->instances++;
	}
	stats->age = system_clock::now();

	lock.lock();
	this->_general_statistics = stats;
	lock.unlock();
	create_lock.unlock();
	return stats;
}

std::shared_ptr<HistoryStatistics> StatisticManager::history(license::stats::HistoryStatistics::HistoryType type) {
	lock_guard<recursive_mutex> lock(this->_history_locks[type]);
	auto current_time = system_clock::now();
	auto& entry = this->_history[type];

	if(entry && entry->evaluated + HistoryStatistics::cache_timeout(type) > current_time)
		return entry;

	entry = make_shared<HistoryStatistics>();

	entry->period = HistoryStatistics::time_period(type);
	entry->begin = HistoryStatistics::align_type(type, current_time - HistoryStatistics::type_duration(type));
	entry->end = HistoryStatistics::align_type(type, current_time);

	if(type == HistoryStatistics::DAY_YESTERDAY || type == HistoryStatistics::DAY_7DAYS_AGO) {
		auto& reference = this->_history[HistoryStatistics::LAST_DAY];
		if(reference) {
			entry->begin = reference->begin;
			entry->end = reference->end;
			entry->evaluated = reference->evaluated;
		}
		if(type == HistoryStatistics::DAY_YESTERDAY) {
			entry->begin -= hours(24);
			entry->end -= hours(24);
		} else if(type == HistoryStatistics::DAY_7DAYS_AGO) {
			entry->begin -= hours(24) * 7;
			entry->end -= hours(24) * 7;
		}
	}

	auto statistics = this->license_manager->list_statistics_user(entry->begin, entry->end, entry->period);

	entry->statistics = std::move(statistics);
	if(entry->evaluated.time_since_epoch().count() == 0)
		entry->evaluated = current_time;

	if(type == HistoryStatistics::LAST_DAY) {
		this->history(HistoryStatistics::DAY_YESTERDAY);
		this->history(HistoryStatistics::DAY_7DAYS_AGO);
	}
	return entry;
}