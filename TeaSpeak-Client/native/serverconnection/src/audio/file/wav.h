#pragma once

#include <string>
#include <fstream>

namespace tc::audio::file {
    enum FileOpenResult {
        OPEN_RESULT_SUCCESS,
        OPEN_RESULT_INVALID_FORMAT,
        OPEN_RESULT_FORMAT_UNSUPPORTED,
        OPEN_RESULT_ERROR
    };

    enum ReadResult {
        READ_RESULT_SUCCESS,
        READ_RESULT_EOF,
        READ_RESULT_UNRECOVERABLE_ERROR
    };

    class WAVReader {
        public:
            explicit WAVReader(std::string  /* path */);
            ~WAVReader();

            [[nodiscard]] inline const std::string& file_path() const { return this->file_path_; }

            [[nodiscard]] FileOpenResult open_file(std::string& /* error */);
            void close_file();

            [[nodiscard]] inline size_t channels() const { return this->channels_; }
            [[nodiscard]] inline size_t sample_rate() const { return this->sample_rate_; }
            [[nodiscard]] inline size_t total_samples() const { return this->total_samples_; }
            [[nodiscard]] inline size_t current_sample_offset() const { return this->current_sample_offset_; }

            /**
             * Audio data in interleaved floats reachting from [-1;1].
             * Must contains at least channels * sizeof(float) * sample_count bytes
             */
            [[nodiscard]] ReadResult read(void* /* target buffer */, size_t* /* sample count */);
        private:
            const std::string file_path_;

            std::ifstream is_{};

            size_t channels_{0};
            size_t sample_rate_{0};
            size_t total_samples_{0};
            size_t current_sample_offset_{0};
            uint8_t bytes_per_sample{0}; /* 1, 2 or 3 (8, 16, 24 bit) */
    };
}