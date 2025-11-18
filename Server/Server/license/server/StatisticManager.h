#pragma once

#include <mutex>
#include <memory>
#include <chrono>
#include "DatabaseHandler.h"

namespace license::stats {
    struct GeneralStatistics {
        std::chrono::system_clock::time_point age;

        uint64_t empty_instances{0};
        uint64_t instances{0};
        uint64_t servers{0};
        uint64_t clients{0};
        uint64_t bots{0};
    };

    struct HistoryStatistics {
        enum HistoryPeriod {
            DAY,
            WEEK,
            MONTH,
            HALF_YEAR,
            YEAR
        };
        enum HistoryOffset {
            NOW,
            ONE_BEFORE,
            SEVEN_BEFORE,
            TWELVE_BEFORE
        };
        enum HistoryType {
            LAST_DAY,
            DAY_YESTERDAY,
            DAY_7DAYS_AGO,
            LAST_WEEK,
            LAST_MONTH,
            LAST_HALF_YEAR
        };
        static std::chrono::system_clock::time_point align_type(HistoryType type, const std::chrono::system_clock::time_point&);
        static std::chrono::milliseconds time_period(HistoryType type);
        static std::chrono::milliseconds cache_timeout(HistoryType type);
        static std::chrono::milliseconds type_duration(HistoryType type);

        std::chrono::system_clock::time_point evaluated;
        std::chrono::system_clock::time_point begin;
        std::chrono::system_clock::time_point end;
        std::chrono::milliseconds period;
        HistoryType type;

        std::shared_ptr<server::database::DatabaseHandler::UserHistory> statistics;
    };

    class StatisticManager {
        public:
            explicit StatisticManager(std::shared_ptr<server::database::DatabaseHandler>  /* manager */);
            virtual ~StatisticManager();

            void reset_cache_general();
            std::shared_ptr<GeneralStatistics> general_statistics();
            std::shared_ptr<HistoryStatistics> history(HistoryStatistics::HistoryType);
        private:
            std::shared_ptr<server::database::DatabaseHandler> license_manager;

            std::recursive_mutex _general_statistics_lock;
            std::recursive_mutex _general_statistics_generate_lock;
            std::shared_ptr<GeneralStatistics> _general_statistics;

            std::map<HistoryStatistics::HistoryType, std::recursive_mutex> _history_locks;
            std::map<HistoryStatistics::HistoryType, std::shared_ptr<HistoryStatistics>> _history;
    };
}