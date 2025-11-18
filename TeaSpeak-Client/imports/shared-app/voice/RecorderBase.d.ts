import { InputDevice } from "../audio/Recorder";
import { Registry } from "../events";
import { Filter, FilterType, FilterTypeClass } from "../voice/Filter";
export declare enum InputConsumerType {
    CALLBACK = 0,
    NODE = 1,
    NATIVE = 2
}
export interface CallbackInputConsumer {
    type: InputConsumerType.CALLBACK;
    callbackAudio?: (buffer: AudioBuffer) => any;
    callbackBuffer?: (buffer: Float32Array, samples: number, channels: number) => any;
}
export interface NodeInputConsumer {
    type: InputConsumerType.NODE;
    callbackNode: (source_node: AudioNode) => any;
    callbackDisconnect: (source_node: AudioNode) => any;
}
export interface NativeInputConsumer {
    type: InputConsumerType.NATIVE;
}
export declare type InputConsumer = CallbackInputConsumer | NodeInputConsumer | NativeInputConsumer;
export declare enum InputState {
    PAUSED = 0,
    INITIALIZING = 1,
    RECORDING = 2
}
export declare enum InputStartError {
    EUNKNOWN = "eunknown",
    EDEVICEUNKNOWN = "edeviceunknown",
    EBUSY = "ebusy",
    ENOTALLOWED = "enotallowed",
    ESYSTEMDENIED = "esystemdenied",
    ENOTSUPPORTED = "enotsupported",
    ESYSTEMUNINITIALIZED = "esystemuninitialized"
}
export interface InputEvents {
    notify_state_changed: {
        oldState: InputState;
        newState: InputState;
    };
    notify_voice_start: {};
    notify_voice_end: {};
    notify_filter_mode_changed: {
        oldMode: FilterMode;
        newMode: FilterMode;
    };
    notify_device_changed: {
        oldDeviceId: string;
        newDeviceId: string;
    };
}
export declare enum FilterMode {
    /**
     * Apply all filters and act according to the output
     */
    Filter = 0,
    /**
     * Bypass all filters and replay the audio
     */
    Bypass = 1,
    /**
     * Block all communication
     */
    Block = 2
}
/**
 * All available options for input processing.
 * Since input processing is only available on the native client these are the options
 * the native client (especially WebRTC audio processing) have.
 */
export interface InputProcessorConfigWebRTC {
    "pipeline.maximum_internal_processing_rate": number;
    "pipeline.multi_channel_render": boolean;
    "pipeline.multi_channel_capture": boolean;
    "pre_amplifier.enabled": boolean;
    "pre_amplifier.fixed_gain_factor": number;
    "high_pass_filter.enabled": boolean;
    "high_pass_filter.apply_in_full_band": boolean;
    "echo_canceller.enabled": boolean;
    "echo_canceller.mobile_mode": boolean;
    "echo_canceller.export_linear_aec_output": boolean;
    "echo_canceller.enforce_high_pass_filtering": boolean;
    "noise_suppression.enabled": boolean;
    "noise_suppression.level": "low" | "moderate" | "high" | "very-high";
    "noise_suppression.analyze_linear_aec_output_when_available": boolean;
    "transient_suppression.enabled": boolean;
    "voice_detection.enabled": boolean;
    "gain_controller1.enabled": boolean;
    "gain_controller1.mode": "adaptive-analog" | "adaptive-digital" | "fixed-digital";
    "gain_controller1.target_level_dbfs": number;
    "gain_controller1.compression_gain_db": number;
    "gain_controller1.enable_limiter": boolean;
    "gain_controller1.analog_level_minimum": number;
    "gain_controller1.analog_level_maximum": number;
    "gain_controller1.analog_gain_controller.enabled": boolean;
    "gain_controller1.analog_gain_controller.startup_min_volume": number;
    "gain_controller1.analog_gain_controller.clipped_level_min": number;
    "gain_controller1.analog_gain_controller.enable_agc2_level_estimator": boolean;
    "gain_controller1.analog_gain_controller.enable_digital_adaptive": boolean;
    "gain_controller2.enabled": boolean;
    "gain_controller2.fixed_digital.gain_db": number;
    "gain_controller2.adaptive_digital.enabled": boolean;
    "gain_controller2.adaptive_digital.vad_probability_attack": number;
    "gain_controller2.adaptive_digital.level_estimator": "rms" | "peak";
    "gain_controller2.adaptive_digital.level_estimator_adjacent_speech_frames_threshold": number;
    "gain_controller2.adaptive_digital.use_saturation_protector": boolean;
    "gain_controller2.adaptive_digital.initial_saturation_margin_db": number;
    "gain_controller2.adaptive_digital.extra_saturation_margin_db": number;
    "gain_controller2.adaptive_digital.gain_applier_adjacent_speech_frames_threshold": number;
    "gain_controller2.adaptive_digital.max_gain_change_db_per_second": number;
    "gain_controller2.adaptive_digital.max_output_noise_level_dbfs": number;
    "residual_echo_detector.enabled": boolean;
    "level_estimation.enabled": boolean;
}
/**
 * Attention:
 * These keys **MUST** be equal to all keys of `InputProcessorConfigWebRTC`.
 * All keys not registered in here will not be consideration.
 */
export declare const kInputProcessorConfigWebRTCKeys: (keyof InputProcessorConfigWebRTC)[];
export interface InputProcessorConfigRNNoise {
    "rnnoise.enabled": boolean;
}
/**
 * Attention:
 * These keys **MUST** be equal to all keys of `InputProcessorConfigWebRTC`.
 * All keys not registered in here will not be consideration.
 */
export declare const kInputProcessorConfigRNNoiseKeys: (keyof InputProcessorConfigRNNoise)[];
export interface InputProcessorConfigMapping {
    "webrtc-processing": InputProcessorConfigWebRTC;
    "rnnoise": InputProcessorConfigRNNoise;
}
export declare type InputProcessorType = keyof InputProcessorConfigMapping;
export interface InputProcessorStatistics {
    output_rms_dbfs: number | undefined;
    voice_detected: number | undefined;
    echo_return_loss: number | undefined;
    echo_return_loss_enhancement: number | undefined;
    divergent_filter_fraction: number | undefined;
    delay_median_ms: number | undefined;
    delay_standard_deviation_ms: number | undefined;
    residual_echo_likelihood: number | undefined;
    residual_echo_likelihood_recent_max: number | undefined;
    delay_ms: number | undefined;
    rnnoise_volume: number | undefined;
}
export interface InputProcessor {
    /**
     * @param processor Target processor type
     * @returns `true` if the target processor type is supported and available
     */
    hasProcessor(processor: InputProcessorType): boolean;
    /**
     * Get the processor config of the target type.
     * This method will throw when the target processor isn't supported.
     * @param processor Target processor type.
     * @returns The processor config.
     */
    getProcessorConfig<T extends InputProcessorType>(processor: T): InputProcessorConfigMapping[T];
    /**
     * Apply the target config.
     * @param processor
     * @param config
     */
    applyProcessorConfig<T extends InputProcessorType>(processor: T, config: InputProcessorConfigMapping[T]): any;
    /**
     * Get the current processor statistics.
     */
    getStatistics(): InputProcessorStatistics;
}
export interface AbstractInput {
    readonly events: Registry<InputEvents>;
    currentState(): InputState;
    destroy(): any;
    start(): Promise<InputStartError | true>;
    stop(): Promise<void>;
    isFiltered(): boolean;
    getFilterMode(): FilterMode;
    setFilterMode(mode: FilterMode): any;
    currentDeviceId(): string | undefined;
    /**
     * This method should not throw!
     * If the target device is unknown, it should return `InputStartError.EDEVICEUNKNOWN` on start.
     * If the device is different than the current device the recorder stops.
     *
     * When the device has been changed the event `notify_device_changed` will be fired.
     */
    setDeviceId(device: string): Promise<void>;
    currentConsumer(): InputConsumer | undefined;
    setConsumer(consumer: InputConsumer): Promise<void>;
    supportsFilter(type: FilterType): boolean;
    createFilter<T extends FilterType>(type: T, priority: number): FilterTypeClass<T>;
    removeFilter(filter: Filter): any;
    getVolume(): number;
    setVolume(volume: number): any;
    getInputProcessor(): InputProcessor;
    /**
     * Create a new level meter for this audio input.
     * This level meter will be indicate the audio level after all processing.
     * Note: Changing the input device or stopping the input will result in no activity.
     */
    createLevelMeter(): LevelMeter;
}
export interface LevelMeter {
    getDevice(): InputDevice;
    setObserver(callback: (value: number) => any): any;
    destroy(): any;
}
