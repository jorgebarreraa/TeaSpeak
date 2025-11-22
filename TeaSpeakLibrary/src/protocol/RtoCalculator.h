#pragma once


namespace ts::protocol {
    class RtoCalculator {
        public:
            explicit RtoCalculator();

            void reset();
            void update(float /* response time */);

            [[nodiscard]] inline auto current_rto() const { return this->rto; }
            [[nodiscard]] inline auto current_srtt() const { return this->srtt; }
            [[nodiscard]] inline auto current_rttvar() const { return this->rttvar; }
        private:
            float rto{1000};
            float srtt{-1};
            float rttvar{};

            constexpr static auto alpha{.125f};
            constexpr static auto beta{.25f};
    };
}