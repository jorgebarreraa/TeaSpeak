//
// Created by WolverinDEV on 28/03/2021.
//

#include "./AudioProcessor.h"
#include "../../logger.h"
#include <NanStrings.h>

using namespace tc::audio;

NAN_MODULE_INIT(AudioProcessorWrapper::Init) {
    auto klass = Nan::New<v8::FunctionTemplate>(AudioProcessorWrapper::NewInstance);
    klass->SetClassName(Nan::New("AudioProcessor").ToLocalChecked());
    klass->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(klass, "get_config", AudioProcessorWrapper::get_config);
    Nan::SetPrototypeMethod(klass, "apply_config", AudioProcessorWrapper::apply_config);

    Nan::SetPrototypeMethod(klass, "get_statistics", AudioProcessorWrapper::get_statistics);

    constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(AudioProcessorWrapper::NewInstance) {
    if(!info.IsConstructCall()) {
        Nan::ThrowError("invalid invoke!");
    }
}

AudioProcessorWrapper::AudioProcessorWrapper(const std::shared_ptr<AudioProcessor> &processor) {
    log_allocate("AudioProcessorWrapper", this);

    this->registered_observer = new Observer{this};
    this->weak_processor = processor;
    processor->register_process_observer(this->registered_observer);
}

AudioProcessorWrapper::~AudioProcessorWrapper() noexcept {
    log_free("AudioProcessorWrapper", this);

    if(auto processor{this->weak_processor.lock()}; processor) {
        processor->unregister_process_observer(this->registered_observer);
    }

    delete this->registered_observer;
}

#define PUT_VALUE(key, value)                                                    \
    result->Set(context, Nan::LocalStringUTF8(#key), value).Check()

#define PUT_CONFIG(path)                                                         \
    PUT_VALUE(path, Nan::New(config.path))

#define LOAD_CONFIG(path, ...)                                                   \
do {                                                                             \
   if(!load_config_value(context, js_config, #path, config.path, ##__VA_ARGS__)) { \
       return;                                                                   \
   }                                                                             \
} while(0)

NAN_METHOD(AudioProcessorWrapper::get_config) {
    auto handle = Nan::ObjectWrap::Unwrap<AudioProcessorWrapper>(info.Holder());
    auto processor = handle->weak_processor.lock();
    if(!processor) {
        Nan::ThrowError("processor passed away");
        return;
    }

    auto config = processor->get_config();
    auto result = Nan::New<v8::Object>();
    auto context = info.GetIsolate()->GetCurrentContext();

    PUT_CONFIG(pipeline.maximum_internal_processing_rate);
    PUT_CONFIG(pipeline.multi_channel_render);
    PUT_CONFIG(pipeline.multi_channel_capture);

    PUT_CONFIG(pre_amplifier.enabled);
    PUT_CONFIG(pre_amplifier.fixed_gain_factor);

    PUT_CONFIG(high_pass_filter.enabled);
    PUT_CONFIG(high_pass_filter.apply_in_full_band);

    PUT_CONFIG(echo_canceller.enabled);
    PUT_CONFIG(echo_canceller.mobile_mode);
    PUT_CONFIG(echo_canceller.export_linear_aec_output); /* TODO: Consider removing? */
    PUT_CONFIG(echo_canceller.enforce_high_pass_filtering);

    PUT_CONFIG(noise_suppression.enabled);
    switch (config.noise_suppression.level) {
        using Level = webrtc::AudioProcessing::Config::NoiseSuppression::Level;
        case Level::kLow:
            PUT_VALUE(noise_suppression.level, Nan::LocalStringUTF8("low"));
            break;

        case Level::kModerate:
            PUT_VALUE(noise_suppression.level, Nan::LocalStringUTF8("moderate"));
            break;

        case Level::kHigh:
            PUT_VALUE(noise_suppression.level, Nan::LocalStringUTF8("high"));
            break;

        case Level::kVeryHigh:
            PUT_VALUE(noise_suppression.level, Nan::LocalStringUTF8("very-high"));
            break;

        default:
            PUT_VALUE(noise_suppression.level, Nan::LocalStringUTF8("unknown"));
            break;
    }
    PUT_CONFIG(noise_suppression.analyze_linear_aec_output_when_available);

    PUT_CONFIG(transient_suppression.enabled);

    PUT_CONFIG(voice_detection.enabled);

    PUT_CONFIG(gain_controller1.enabled);
    switch (config.gain_controller1.mode) {
        using Mode = webrtc::AudioProcessing::Config::GainController1::Mode;
        case Mode::kAdaptiveAnalog:
            PUT_VALUE(gain_controller1.mode, Nan::LocalStringUTF8("adaptive-analog"));
            break;

        case Mode::kAdaptiveDigital:
            PUT_VALUE(gain_controller1.mode, Nan::LocalStringUTF8("adaptive-digital"));
            break;

        case Mode::kFixedDigital:
            PUT_VALUE(gain_controller1.mode, Nan::LocalStringUTF8("fixed-digital"));
            break;

        default:
            PUT_VALUE(gain_controller1.mode, Nan::LocalStringUTF8("unknown"));
            break;
    }
    PUT_CONFIG(gain_controller1.target_level_dbfs);
    PUT_CONFIG(gain_controller1.compression_gain_db);
    PUT_CONFIG(gain_controller1.enable_limiter);
    PUT_CONFIG(gain_controller1.analog_level_minimum);
    PUT_CONFIG(gain_controller1.analog_level_maximum);

    PUT_CONFIG(gain_controller1.analog_gain_controller.enabled);
    PUT_CONFIG(gain_controller1.analog_gain_controller.startup_min_volume);
    PUT_CONFIG(gain_controller1.analog_gain_controller.clipped_level_min);
    PUT_CONFIG(gain_controller1.analog_gain_controller.enable_agc2_level_estimator);
    PUT_CONFIG(gain_controller1.analog_gain_controller.enable_digital_adaptive);

    PUT_CONFIG(gain_controller2.enabled);

    PUT_CONFIG(gain_controller2.fixed_digital.gain_db);

    PUT_CONFIG(gain_controller2.adaptive_digital.enabled);
    switch(config.gain_controller2.adaptive_digital.level_estimator) {
        using LevelEstimator = webrtc::AudioProcessing::Config::GainController2::LevelEstimator;

        case LevelEstimator::kPeak:
            PUT_VALUE(gain_controller2.adaptive_digital.level_estimator, Nan::LocalStringUTF8("peak"));
            break;

        case LevelEstimator::kRms:
            PUT_VALUE(gain_controller2.adaptive_digital.level_estimator, Nan::LocalStringUTF8("rms"));
            break;

        default:
            PUT_VALUE(gain_controller2.adaptive_digital.level_estimator, Nan::LocalStringUTF8("unknown"));
            break;
    }
    PUT_CONFIG(gain_controller2.adaptive_digital.vad_probability_attack);
    PUT_CONFIG(gain_controller2.adaptive_digital.level_estimator_adjacent_speech_frames_threshold);
    PUT_CONFIG(gain_controller2.adaptive_digital.use_saturation_protector);
    PUT_CONFIG(gain_controller2.adaptive_digital.initial_saturation_margin_db);
    PUT_CONFIG(gain_controller2.adaptive_digital.extra_saturation_margin_db);
    PUT_CONFIG(gain_controller2.adaptive_digital.gain_applier_adjacent_speech_frames_threshold);
    PUT_CONFIG(gain_controller2.adaptive_digital.max_gain_change_db_per_second);
    PUT_CONFIG(gain_controller2.adaptive_digital.max_output_noise_level_dbfs);

    PUT_CONFIG(residual_echo_detector.enabled);
    PUT_CONFIG(level_estimation.enabled);
    PUT_CONFIG(rnnoise.enabled);
    PUT_CONFIG(artificial_stream_delay);

    info.GetReturnValue().Set(result);
}

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
inline bool load_config_value(
        const v8::Local<v8::Context>& context,
        const v8::Local<v8::Object>& js_config,
        const std::string_view& key,
        T& value_ref,
        T min_value = std::numeric_limits<T>::min(),
        T max_value = std::numeric_limits<T>::max()
) {
    auto maybe_value = js_config->Get(context, Nan::LocalStringUTF8(key));
    if(maybe_value.IsEmpty() || maybe_value.ToLocalChecked()->IsNullOrUndefined()) {
        return true;
    }

    double value;

    if(maybe_value.ToLocalChecked()->IsNumber()) {
        value = maybe_value.ToLocalChecked()->NumberValue(context).ToChecked();
    } else if(maybe_value.ToLocalChecked()->IsBoolean()) {
        value = maybe_value.ToLocalChecked()->BooleanValue(v8::Isolate::GetCurrent());
    } else {
        Nan::ThrowError(Nan::LocalStringUTF8("property " + std::string{key} + " isn't a number or boolean"));
        return false;
    }

    if(std::numeric_limits<T>::is_integer && (double) (T) value != value) {
        Nan::ThrowError(Nan::LocalStringUTF8("property " + std::string{key} + " isn't an integer"));
        return false;
    }

    if((T) value < min_value) {
        Nan::ThrowError(Nan::LocalStringUTF8("property " + std::string{key} + " exceeds min value of " + std::to_string((T) min_value) + " (value: " + std::to_string((T) value) + ")"));
        return false;
    }

    if((T) value > (double) max_value) {
        Nan::ThrowError(Nan::LocalStringUTF8("property " + std::string{key} + " exceeds max value of " + std::to_string((T) max_value) + " (value: " + std::to_string((T) value) + ")"));
        return false;
    }

    value_ref = value;
    return true;
}

template <size_t kValueSize, typename T>
inline bool load_config_enum(
        const v8::Local<v8::Context>& context,
        const v8::Local<v8::Object>& js_config,
        const std::string_view& key,
        T& value_ref,
        const std::array<std::pair<std::string_view, T>, kValueSize>& values
) {
    auto maybe_value = js_config->Get(context, Nan::LocalStringUTF8(key));
    if(maybe_value.IsEmpty() || maybe_value.ToLocalChecked()->IsNullOrUndefined()) {
        return true;
    } else if(!maybe_value.ToLocalChecked()->IsString()) {
        Nan::ThrowError(Nan::LocalStringUTF8("property " + std::string{key} + " isn't a string"));
        return false;
    }

    auto str_value = maybe_value.ToLocalChecked()->ToString(context).ToLocalChecked();
    auto value = *Nan::Utf8String(str_value);
    for(const auto& [ key, key_value ] : values) {
        if(key != value) {
            continue;
        }

        value_ref = key_value;
        return true;
    }

    Nan::ThrowError(Nan::LocalStringUTF8("property " + std::string{key} + " contains an invalid enum value (" + value + ")"));
    return false;
}

#define LOAD_ENUM(path, arg_count, ...)                                                                 \
do {                                                                                                    \
    if(!load_config_enum<arg_count>(context, js_config, #path, config.path, {{ __VA_ARGS__ }})) {       \
        return;                                                                                         \
    }                                                                                                   \
} while(0)

NAN_METHOD(AudioProcessorWrapper::apply_config) {
    auto handle = Nan::ObjectWrap::Unwrap<AudioProcessorWrapper>(info.Holder());
    auto processor = handle->weak_processor.lock();
    if (!processor) {
        Nan::ThrowError("processor passed away");
        return;
    }

    if(info.Length() != 1 || !info[0]->IsObject()) {
        Nan::ThrowError("Invalid arguments");
        return;
    }

    auto config = processor->get_config();
    auto context = info.GetIsolate()->GetCurrentContext();
    auto js_config = info[0]->ToObject(info.GetIsolate()->GetCurrentContext()).ToLocalChecked();

    using GainControllerMode = webrtc::AudioProcessing::Config::GainController1::Mode;
    using GainControllerLevelEstimator = webrtc::AudioProcessing::Config::GainController2::LevelEstimator;
    using NoiseSuppressionLevel = webrtc::AudioProcessing::Config::NoiseSuppression::Level;

    LOAD_CONFIG(pipeline.maximum_internal_processing_rate);
    LOAD_CONFIG(pipeline.multi_channel_render);
    LOAD_CONFIG(pipeline.multi_channel_capture);

    LOAD_CONFIG(pre_amplifier.enabled);
    LOAD_CONFIG(pre_amplifier.fixed_gain_factor);

    LOAD_CONFIG(high_pass_filter.enabled);
    LOAD_CONFIG(high_pass_filter.apply_in_full_band);

    LOAD_CONFIG(echo_canceller.enabled);
    LOAD_CONFIG(echo_canceller.mobile_mode);
    LOAD_CONFIG(echo_canceller.export_linear_aec_output); /* TODO: Consider removing? */
    LOAD_CONFIG(echo_canceller.enforce_high_pass_filtering);

    LOAD_CONFIG(noise_suppression.enabled);
    LOAD_ENUM(noise_suppression.level, 4,
              { "low", NoiseSuppressionLevel::kLow },
              { "moderate", NoiseSuppressionLevel::kModerate },
              { "high", NoiseSuppressionLevel::kHigh },
              { "very-high", NoiseSuppressionLevel::kVeryHigh }
    );
    LOAD_CONFIG(noise_suppression.analyze_linear_aec_output_when_available);

    LOAD_CONFIG(transient_suppression.enabled);

    LOAD_CONFIG(voice_detection.enabled);

    LOAD_CONFIG(gain_controller1.enabled);
    LOAD_ENUM(gain_controller1.mode, 3,
              { "adaptive-analog", GainControllerMode::kAdaptiveAnalog },
              { "adaptive-digital", GainControllerMode::kAdaptiveDigital },
              { "fixed-digital", GainControllerMode::kFixedDigital }
    );
    LOAD_CONFIG(gain_controller1.target_level_dbfs);
    LOAD_CONFIG(gain_controller1.compression_gain_db);
    LOAD_CONFIG(gain_controller1.enable_limiter);
    LOAD_CONFIG(gain_controller1.analog_level_minimum);
    LOAD_CONFIG(gain_controller1.analog_level_maximum);

    LOAD_CONFIG(gain_controller1.analog_gain_controller.enabled);
    LOAD_CONFIG(gain_controller1.analog_gain_controller.startup_min_volume);
    LOAD_CONFIG(gain_controller1.analog_gain_controller.clipped_level_min);
    LOAD_CONFIG(gain_controller1.analog_gain_controller.enable_agc2_level_estimator);
    LOAD_CONFIG(gain_controller1.analog_gain_controller.enable_digital_adaptive);

    LOAD_CONFIG(gain_controller2.enabled);

    LOAD_CONFIG(gain_controller2.fixed_digital.gain_db);

    LOAD_CONFIG(gain_controller2.adaptive_digital.enabled);
    LOAD_ENUM(gain_controller2.adaptive_digital.level_estimator, 2,
              { "peak", GainControllerLevelEstimator::kPeak },
              { "rms", GainControllerLevelEstimator::kRms }
    );
    LOAD_CONFIG(gain_controller2.adaptive_digital.vad_probability_attack);
    LOAD_CONFIG(gain_controller2.adaptive_digital.level_estimator_adjacent_speech_frames_threshold);
    LOAD_CONFIG(gain_controller2.adaptive_digital.use_saturation_protector);
    LOAD_CONFIG(gain_controller2.adaptive_digital.initial_saturation_margin_db);
    LOAD_CONFIG(gain_controller2.adaptive_digital.extra_saturation_margin_db);
    LOAD_CONFIG(gain_controller2.adaptive_digital.gain_applier_adjacent_speech_frames_threshold);
    LOAD_CONFIG(gain_controller2.adaptive_digital.max_gain_change_db_per_second);
    LOAD_CONFIG(gain_controller2.adaptive_digital.max_output_noise_level_dbfs);

    LOAD_CONFIG(residual_echo_detector.enabled);
    LOAD_CONFIG(level_estimation.enabled);
    LOAD_CONFIG(rnnoise.enabled);
    LOAD_CONFIG(artificial_stream_delay);

    processor->apply_config(config);
}

#define PUT_STATISTIC(path)                         \
do {                                                \
    if(config.path.has_value()) {                   \
        PUT_VALUE(path, Nan::New(*config.path));    \
    } else {                                        \
        PUT_VALUE(path, Nan::Undefined());          \
    }                                               \
} while(0)

NAN_METHOD(AudioProcessorWrapper::get_statistics) {
    auto handle = Nan::ObjectWrap::Unwrap<AudioProcessorWrapper>(info.Holder());
    auto processor = handle->weak_processor.lock();
    if(!processor) {
        Nan::ThrowError("processor passed away");
        return;
    }

    auto config = processor->get_statistics();
    auto result = Nan::New<v8::Object>();
    auto context = info.GetIsolate()->GetCurrentContext();

    PUT_STATISTIC(output_rms_dbfs);
    PUT_STATISTIC(voice_detected);
    PUT_STATISTIC(echo_return_loss);
    PUT_STATISTIC(echo_return_loss_enhancement);
    PUT_STATISTIC(divergent_filter_fraction);
    PUT_STATISTIC(delay_median_ms);
    PUT_STATISTIC(delay_standard_deviation_ms);
    PUT_STATISTIC(residual_echo_likelihood);
    PUT_STATISTIC(residual_echo_likelihood_recent_max);
    PUT_STATISTIC(delay_ms);
    PUT_STATISTIC(rnnoise_volume);

    info.GetReturnValue().Set(result);
}

void AudioProcessorWrapper::Observer::stream_processed(const AudioProcessor::Stats &stats) {
    /* TODO! */
}