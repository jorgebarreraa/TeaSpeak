export enum ServerType {
    UNKNOWN,
    TEASPEAK,
    TEAMSPEAK
}

export enum PlayerState {
    BUFFERING,
    PLAYING,
    STOPPING,
    STOPPED
}

export interface NativeVoiceClient {
    client_id: number;

    callback_playback: () => any;
    callback_stopped: () => any;

    callback_state_changed: (new_state: PlayerState) => any;

    get_state() : PlayerState;

    get_volume() : number;
    set_volume(volume: number) : void;

    abort_replay();
    get_stream() : audio.playback.AudioOutputStream;
}

export interface NativeVoiceConnection {
    register_client(client_id: number) : NativeVoiceClient;
    available_clients() : NativeVoiceClient[];
    unregister_client(client_id: number);

    audio_source() : audio.record.AudioConsumer;
    set_audio_source(consumer: audio.record.AudioConsumer | undefined);

    decoding_supported(codec: number) : boolean;
    encoding_supported(codec: number) : boolean;

    get_encoder_codec() : number;
    set_encoder_codec(codec: number);

    /* could may throw an exception when the underlying voice sender has been deallocated */
    enable_voice_send(flag: boolean);
}

export interface NativeServerConnection {
    callback_voice_data: (buffer: Uint8Array, client_id: number, codec_id: number, flag_head: boolean, packet_id: number) => any;
    callback_command: (command: string, arguments: any, switches: string[]) => any;
    callback_disconnect: (reason?: string) => any;
    _voice_connection: NativeVoiceConnection;
    server_type: ServerType;

    connected(): boolean;

    connect(properties: {
        remote_host: string,
        remote_port: number,

        timeout: number,

        callback: (error: number) => any,

        identity_key: string | undefined, /* if identity_key empty, then an ephemeral license will be created */
        teamspeak: boolean
    });

    disconnect(reason: string | undefined, callback: (error: number) => any) : boolean;

    error_message(id: number) : string;

    send_command(command: string, arguments: any[], switches: string[]);
    send_voice_data(buffer: Uint8Array, codec_id: number, header: boolean);
    send_voice_data_raw(buffer: Float32Array, channels: number, sample_rate: number, header: boolean);

    /* ping in microseconds */
    current_ping() : number;

    statistics() : { voice_bytes_received: number, voice_bytes_send: number, control_bytes_received: number, control_bytes_send } | undefined
}

export function spawn_server_connection() : NativeServerConnection;
export function destroy_server_connection(connection: NativeServerConnection);

export namespace ft {
    export interface TransferObject {
        name: string;
        direction: "upload" | "download";
    }

    export interface FileTransferSource extends TransferObject {
        total_size: number;
    }

    export interface FileTransferTarget extends TransferObject  {
        expected_size: number;
    }

    export interface NativeFileTransfer {
        handle: TransferObject;

        callback_finished: (aborted?: boolean) => any;
        callback_start: () => any;
        callback_progress: (current: number, max: number) => any;
        callback_failed: (message: string) => any;

        /**
         * @return true if the connect method has been executed successfully
         *         false if the connect fails, callback_failed will be called with the exact reason
         */
        start() : boolean;
    }

    export interface TransferOptions {
        remote_address: string;
        remote_port: number;

        transfer_key: string;
        client_transfer_id: number;
        server_transfer_id: number;

        object: TransferObject;
    }

    export function upload_transfer_object_from_file(path: string, name: string) : FileTransferSource;
    export function upload_transfer_object_from_buffer(buffer: ArrayBuffer) : FileTransferSource;

    export function download_transfer_object_from_buffer(target_buffer: ArrayBuffer) : FileTransferTarget;
    export function download_transfer_object_from_file(path: string, name: string, expectedSize: number) : FileTransferTarget;

    export function destroy_connection(connection: NativeFileTransfer);
    export function spawn_connection(transfer: TransferOptions) : NativeFileTransfer;
}

export namespace audio {
    export interface AudioDevice {
        name: string;
        driver: string;

        device_id: string;

        input_supported: boolean;
        output_supported: boolean;

        input_default: boolean;
        output_default: boolean;
    }

    export namespace playback {
        export interface AudioOutputStream {
            sample_rate: number;
            channels: number;

            get_buffer_latency() : number;
            set_buffer_latency(value: number);

            get_buffer_max_latency() : number;
            set_buffer_max_latency(value: number);

            flush_buffer();
        }

        export interface OwnedAudioOutputStream extends AudioOutputStream {
            callback_underflow: () => any;
            callback_overflow: () => any;

            clear();
            write_data(buffer: ArrayBuffer, interleaved: boolean);
            write_data_rated(buffer: ArrayBuffer, interleaved: boolean, sample_rate: number);

            deleted() : boolean;
            delete();
        }

        export function set_device(device_id: string);
        export function current_device() : string;

        export function create_stream() : OwnedAudioOutputStream;

        export function get_master_volume() : number;
        export function set_master_volume(volume: number);
    }

    export namespace record {
        enum FilterMode {
            Bypass,
            Filter,
            Block
        }

        export interface ConsumeFilter {
            get_name() : string;
        }

        export interface MarginedFilter {
            /* in seconds */
            get_margin_time() : number;
            set_margin_time(value: number);
        }

        export interface VADConsumeFilter extends ConsumeFilter, MarginedFilter {
            get_level() : number;
        }

        export interface ThresholdConsumeFilter extends ConsumeFilter, MarginedFilter {
            get_threshold() : number;
            set_threshold(value: number);

            get_attack_smooth() : number;
            set_attack_smooth(value: number);

            get_release_smooth() : number;
            set_release_smooth(value: number);

            set_analyze_filter(callback: (value: number) => any);
        }

        export interface StateConsumeFilter extends ConsumeFilter {
            is_consuming() : boolean;
            set_consuming(flag: boolean);
        }

        export interface AudioConsumer {
            readonly sampleRate: number;
            readonly channelCount: number;

            get_filters() : ConsumeFilter[];
            unregister_filter(filter: ConsumeFilter);

            create_filter_vad(level: number) : VADConsumeFilter;
            create_filter_threshold(threshold: number) : ThresholdConsumeFilter;
            create_filter_state() : StateConsumeFilter;

            set_filter_mode(mode: FilterMode);
            get_filter_mode() : FilterMode;

            callback_data: (buffer: Float32Array) => any;
            callback_ended: () => any;
            callback_started: () => any;
        }

        export interface AudioProcessorConfig {
            "pipeline.maximum_internal_processing_rate": number,
            "pipeline.multi_channel_render": boolean,
            "pipeline.multi_channel_capture": boolean,

            "pre_amplifier.enabled": boolean,
            "pre_amplifier.fixed_gain_factor": number,

            "high_pass_filter.enabled": boolean,
            "high_pass_filter.apply_in_full_band": boolean,

            "echo_canceller.enabled": boolean,
            "echo_canceller.mobile_mode": boolean,
            "echo_canceller.export_linear_aec_output": boolean,
            "echo_canceller.enforce_high_pass_filtering": boolean,

            "noise_suppression.enabled": boolean,
            "noise_suppression.level": "low" | "moderate" | "high" | "very-high",
            "noise_suppression.analyze_linear_aec_output_when_available": boolean,

            "transient_suppression.enabled": boolean,

            "voice_detection.enabled": boolean,

            "gain_controller1.enabled": boolean,
            "gain_controller1.mode": "adaptive-analog" | "adaptive-digital" | "fixed-digital",
            "gain_controller1.target_level_dbfs": number,
            "gain_controller1.compression_gain_db": number,
            "gain_controller1.enable_limiter": boolean,
            "gain_controller1.analog_level_minimum": number,
            "gain_controller1.analog_level_maximum": number,

            "gain_controller1.analog_gain_controller.enabled": boolean,
            "gain_controller1.analog_gain_controller.startup_min_volume": number,
            "gain_controller1.analog_gain_controller.clipped_level_min": number,
            "gain_controller1.analog_gain_controller.enable_agc2_level_estimator": boolean,
            "gain_controller1.analog_gain_controller.enable_digital_adaptive": boolean,

            "gain_controller2.enabled": boolean,

            "gain_controller2.fixed_digital.gain_db": number,

            "gain_controller2.adaptive_digital.enabled": boolean,
            "gain_controller2.adaptive_digital.vad_probability_attack": number,
            "gain_controller2.adaptive_digital.level_estimator": "rms" | "peak",
            "gain_controller2.adaptive_digital.level_estimator_adjacent_speech_frames_threshold": number,
            "gain_controller2.adaptive_digital.use_saturation_protector": boolean,
            "gain_controller2.adaptive_digital.initial_saturation_margin_db": number,
            "gain_controller2.adaptive_digital.extra_saturation_margin_db": number,
            "gain_controller2.adaptive_digital.gain_applier_adjacent_speech_frames_threshold": number,
            "gain_controller2.adaptive_digital.max_gain_change_db_per_second": number,
            "gain_controller2.adaptive_digital.max_output_noise_level_dbfs": number,

            "residual_echo_detector.enabled": boolean,
            "level_estimation.enabled": boolean,
            "rnnoise.enabled": boolean,
            "artificial_stream_delay": number
        }

        export interface AudioProcessorStatistics {
            output_rms_dbfs: number | undefined,
            voice_detected: number | undefined,
            echo_return_loss: number | undefined,
            echo_return_loss_enhancement: number | undefined,
            divergent_filter_fraction: number | undefined,
            delay_median_ms: number | undefined,
            delay_standard_deviation_ms: number | undefined,
            residual_echo_likelihood: number | undefined,
            residual_echo_likelihood_recent_max: number | undefined,
            delay_ms: number | undefined,
            rnnoise_volume: number | undefined
        }

        export interface AudioProcessor {
            get_config() : AudioProcessorConfig;
            apply_config(config: Partial<AudioProcessorConfig>);

            get_statistics() : AudioProcessorStatistics;
        }

        export type DeviceSetResult = "success" | "invalid-device";
        export interface AudioRecorder {
            get_device() : string;
            set_device(deviceId: string, callback: (result: DeviceSetResult) => void); /* Recorder needs to be started afterwards */

            start(callback: (result: boolean | string) => void);
            started() : boolean;
            stop();

            get_volume() : number;
            set_volume(volume: number);

            create_consumer() : AudioConsumer;
            get_consumers() : AudioConsumer[];
            delete_consumer(consumer: AudioConsumer);

            get_audio_processor() : AudioProcessor | undefined;
            create_level_meter(mode: "pre-process" | "post-process") : AudioLevelMeter;
        }

        export interface AudioLevelMeter {
            start(callback: (error?: string) => void);
            running() : boolean;
            stop();

            set_callback(callback: (level: number) => void, updateInterval?: number);
        }

        export function create_device_level_meter(targetDeviceId: string) : AudioLevelMeter;
        export function create_recorder() : AudioRecorder;
    }

    export namespace sounds {
        export enum PlaybackResult {
            SUCCEEDED,
            CANCELED,
            SOUND_NOT_INITIALIZED,
            FILE_OPEN_ERROR,
            PLAYBACK_ERROR
        }

        export interface PlaybackSettings {
            file: string;
            volume?: number;
            callback?: (result: PlaybackResult, message: string) => void;
        }
        export function playback_sound(settings: PlaybackSettings) : number;
        export function cancel_playback(playback: number);
    }

    export function initialize(callback: () => any);
    export function initialized() : boolean;
    export function available_devices() : AudioDevice[];
}