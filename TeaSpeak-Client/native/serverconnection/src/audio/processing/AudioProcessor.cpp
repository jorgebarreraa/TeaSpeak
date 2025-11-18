//
// Created by WolverinDEV on 27/03/2021.
//

#include "./AudioProcessor.h"
#include "../../logger.h"
#include <rnnoise.h>
#include <sstream>

using namespace tc::audio;

AudioProcessor::AudioProcessor() {
    this->current_config.echo_canceller.enabled = true;
    this->current_config.echo_canceller.mobile_mode = false;

    this->current_config.gain_controller1.enabled = true;
    this->current_config.gain_controller1.mode = webrtc::AudioProcessing::Config::GainController1::kAdaptiveAnalog;
    this->current_config.gain_controller1.analog_level_minimum = 0;
    this->current_config.gain_controller1.analog_level_maximum = 255;

    this->current_config.gain_controller2.enabled = true;

    this->current_config.high_pass_filter.enabled = true;

    this->current_config.voice_detection.enabled = true;
}

AudioProcessor::~AudioProcessor() {
    std::lock_guard processor_lock{this->processor_mutex};
    delete this->processor;

    for(auto& entry : this->rnnoise_processor) {
        if(!entry) { continue; }

        rnnoise_destroy((DenoiseState*) entry);
    }
}

constexpr static inline auto process_error_to_string(int error) {
    switch (error) {
        case 0: return "kNoError";
        case -1: return "kUnspecifiedError";
        case -2: return "kCreationFailedError";
        case -3: return "kUnsupportedComponentError";
        case -4: return "kUnsupportedFunctionError";
        case -5: return "kNullPointerError";
        case -6: return "kBadParameterError";
        case -7: return "kBadSampleRateError";
        case -8: return "kBadDataLengthError";
        case -9: return "kBadNumberChannelsError";
        case -10: return "kFileError";
        case -11: return "kStreamParameterNotSetError";
        case -12: return "kNotEnabledError";
        case -13: return "kBadStreamParameterWarning";
        default: return "unkown error code";
    }
}

template <typename T>
inline std::ostream& operator<<(std::ostream &ss, const absl::optional<T>& optional) {
    if(optional.has_value()) {
        ss << "optional{" << *optional << "}";
    } else {
        ss << "nullopt";
    }
    return ss;
}

inline std::string statistics_to_string(const webrtc::AudioProcessingStats& stats) {
    std::stringstream ss{};

    ss << "AudioProcessingStats{";
    ss << "output_rms_dbfs: " << stats.output_rms_dbfs << ", ";
    ss << "voice_detected: " << stats.voice_detected << ", ";
    ss << "echo_return_loss: " << stats.echo_return_loss << ", ";
    ss << "echo_return_loss_enhancement: " << stats.echo_return_loss_enhancement << ", ";
    ss << "divergent_filter_fraction: " << stats.divergent_filter_fraction << ", ";
    ss << "delay_median_ms: " << stats.delay_median_ms << ", ";
    ss << "delay_standard_deviation_ms: " << stats.delay_standard_deviation_ms << ", ";
    ss << "residual_echo_likelihood: " << stats.residual_echo_likelihood << ", ";
    ss << "residual_echo_likelihood_recent_max: " << stats.residual_echo_likelihood_recent_max << ", ";
    ss << "delay_ms: " << stats.delay_ms;
    ss << "}";

    return ss.str();
}

bool AudioProcessor::initialize() {
    std::lock_guard processor_lock{this->processor_mutex};
    if(this->processor) {
        /* double initialize */
        return false;
    }

    using namespace webrtc;

    AudioProcessingBuilder builder{};
    this->processor = builder.Create();
    if(!this->processor) {
        return false;
    }

    this->apply_config_unlocked(this->current_config);
    this->processor->Initialize();

    return true;
}

AudioProcessor::Config AudioProcessor::get_config() const {
    std::shared_lock processor_lock{this->processor_mutex};
    return this->current_config;
}

void AudioProcessor::apply_config(const AudioProcessor::Config &config) {
    std::lock_guard processor_lock{this->processor_mutex};
    this->apply_config_unlocked(config);
}

void AudioProcessor::apply_config_unlocked(const Config &config) {
    this->current_config = config;

    /* These are internal bounds and should not be changed */
    this->current_config.gain_controller1.analog_level_minimum = 0;
    this->current_config.gain_controller1.analog_level_maximum = 255;

    if(this->processor) {
        this->processor->ApplyConfig(config);
    }

    if(!this->current_config.rnnoise.enabled) {
        this->rnnoise_volume = absl::nullopt;
    }
    log_trace(category::audio, tr("Applying process config:\n{}\nRNNoise: {}\nArtificial stream delay: {}"), config.ToString(), this->current_config.rnnoise.enabled, this->current_config.artificial_stream_delay);
}

AudioProcessor::Stats AudioProcessor::get_statistics() const {
    std::shared_lock processor_lock{this->processor_mutex};
    return this->get_statistics_unlocked();
}

AudioProcessor::Stats AudioProcessor::get_statistics_unlocked() const {
    if(!this->processor) {
        return AudioProcessor::Stats{};
    }

    AudioProcessor::Stats result{this->processor->GetStatistics()};
    result.rnnoise_volume = this->rnnoise_volume;
    return result;
}

void AudioProcessor::register_process_observer(ProcessObserver *observer) {
    std::lock_guard processor_lock{this->processor_mutex};
    this->process_observer.push_back(observer);
}

bool AudioProcessor::unregister_process_observer(ProcessObserver *observer) {
    std::lock_guard processor_lock{this->processor_mutex};
    auto index = std::find(this->process_observer.begin(), this->process_observer.end(), observer);
    if(index == this->process_observer.end()) {
        return false;
    }

    this->process_observer.erase(index);
    return true;
}

void AudioProcessor::analyze_reverse_stream(const float *const *data, const webrtc::StreamConfig &reverse_config) {
    std::shared_lock processor_lock{this->processor_mutex};
    if(!this->processor) {
        return;
    }

    auto result = this->processor->AnalyzeReverseStream(data, reverse_config);
    if(result != webrtc::AudioProcessing::kNoError) {
        log_error(category::audio, tr("Failed to process reverse stream: {}"), process_error_to_string(result));
        return;
    }
}

std::optional<AudioProcessor::Stats> AudioProcessor::process_stream(const webrtc::StreamConfig &config, float *const *buffer) {

    std::shared_lock processor_lock{this->processor_mutex};
    if(!this->processor) {
        return std::nullopt;
    } else if(config.num_channels() > kMaxChannelCount) {
        log_error(category::audio, tr("AudioProcessor received input buffer with too many channels ({} channels but supported is only {})"), config.num_channels(), kMaxChannelCount);
        return std::nullopt;
    }

    if(this->current_config.rnnoise.enabled) {
        if(config.sample_rate_hz() != 48000) {
            log_warn(category::audio, tr("Don't apply RNNoise. Source sample rate isn't 480kHz ({}kHz)"), config.sample_rate_hz() / 1000);
            this->rnnoise_volume.reset();
        } else {
            static const float kRnNoiseScale = -INT16_MIN;

            double volume_sum{0};
            for(size_t channel{0}; channel < config.num_channels(); channel++) {
                if(!this->rnnoise_processor[channel]) {
                    this->rnnoise_processor[channel] = (void*) rnnoise_create(nullptr);
                }

                {
                    /* RNNoise uses a frame size of 10ms for 48kHz aka 480 samples */
                    auto buffer_ptr = buffer[channel];
                    for(size_t sample{0}; sample < 480; sample++) {
                        *buffer_ptr++ *= kRnNoiseScale;
                    }
                }

                volume_sum += rnnoise_process_frame((DenoiseState*) this->rnnoise_processor[channel], buffer[channel], buffer[channel]);

                {
                    auto buffer_ptr = buffer[channel];
                    for(size_t sample{0}; sample < 480; sample++) {
                        *buffer_ptr++ /= kRnNoiseScale;
                    }
                }
            }

            this->rnnoise_volume = absl::make_optional(volume_sum / config.num_channels());
        }
    }

    /* TODO: Measure it and not just guess it! */
    this->processor->set_stream_delay_ms(this->current_config.artificial_stream_delay);
    if(this->current_config.gain_controller1.enabled) {
        /* TODO: Calculate the actual audio volume */
        this->processor->set_stream_analog_level(0);
    }

    auto result = this->processor->ProcessStream(buffer, config, config, buffer);
    if(result != webrtc::AudioProcessing::kNoError) {
        log_error(category::audio, tr("Failed to process stream: {}"), process_error_to_string(result));
        return std::nullopt;
    }

    auto statistics = this->get_statistics_unlocked();
    for(const auto& observer : this->process_observer) {
        observer->stream_processed(statistics);
    }

    //log_trace(category::audio, tr("Processing stats: {}"), statistics_to_string(statistics));
    return std::make_optional(std::move(statistics));
}