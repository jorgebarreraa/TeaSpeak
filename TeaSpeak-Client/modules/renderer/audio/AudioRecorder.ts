import {
    AbstractInput,
    FilterMode,
    InputConsumer,
    InputConsumerType,
    InputEvents,
    InputProcessor,
    InputProcessorConfigMapping,
    InputProcessorStatistics,
    InputProcessorType,
    InputStartError,
    InputState,
    kInputProcessorConfigRNNoiseKeys,
    kInputProcessorConfigWebRTCKeys,
    LevelMeter,
} from "tc-shared/voice/RecorderBase";
import {audio} from "tc-native/connection";
import {tr} from "tc-shared/i18n/localize";
import {Registry} from "tc-shared/events";
import {Filter, FilterType, FilterTypeClass} from "tc-shared/voice/Filter";
import {NativeFilter, NStateFilter, NThresholdFilter, NVoiceLevelFilter} from "./AudioFilter";
import {getRecorderBackend, InputDevice} from "tc-shared/audio/Recorder";
import {LogCategory, logError, logTrace, logWarn} from "tc-shared/log";
import {Settings, settings} from "tc-shared/settings";
import NativeFilterMode = audio.record.FilterMode;
import AudioProcessor = audio.record.AudioProcessor;

export class NativeInput implements AbstractInput {
    readonly events: Registry<InputEvents>;

    private readonly inputProcessor: NativeInputProcessor;
    private readonly nativeHandle: audio.record.AudioRecorder;
    private readonly nativeConsumer: audio.record.AudioConsumer;

    private listenerRNNoise: () => void;
    private state: InputState;
    private deviceId: string | undefined;

    private registeredFilters: (Filter & NativeFilter)[] = [];
    private filtered = false;

    constructor() {
        this.events = new Registry<InputEvents>();

        this.nativeHandle = audio.record.create_recorder();
        this.inputProcessor = new NativeInputProcessor(this.nativeHandle.get_audio_processor());
        this.inputProcessor.applyProcessorConfig("rnnoise", { "rnnoise.enabled": settings.getValue(Settings.KEY_RNNOISE_FILTER) });
        this.listenerRNNoise = settings.globalChangeListener(Settings.KEY_RNNOISE_FILTER, newValue => this.inputProcessor.applyProcessorConfig("rnnoise", { "rnnoise.enabled": newValue }));

        this.nativeConsumer = this.nativeHandle.create_consumer();

        this.nativeConsumer.callback_ended = () => {
            this.filtered = true;
            this.events.fire("notify_voice_end");
        };
        this.nativeConsumer.callback_started = () => {
            this.filtered = false;
            this.events.fire("notify_voice_start");
        };

        this.state = InputState.PAUSED;
    }

    destroy() {
        if(this.listenerRNNoise) {
            this.listenerRNNoise();
            this.listenerRNNoise = undefined;
        }
    }

    private setState(newState: InputState) {
        if(this.state === newState) {
            return;
        }

        const oldState = this.state;
        this.state = newState;
        this.events.fire("notify_state_changed", { oldState, newState });
    }

    async start(): Promise<InputStartError | true> {
        if(this.state !== InputState.PAUSED) {
            logWarn(LogCategory.VOICE, tr("Input isn't paused"));
            return InputStartError.EBUSY;
        }

        this.setState(InputState.INITIALIZING);
        logTrace(LogCategory.AUDIO, tr("Starting input for device %o", this.deviceId));
        try {
            let deviceId;
            if(this.deviceId === InputDevice.NoDeviceId) {
                throw tr("no device selected");
            } else if(this.deviceId === InputDevice.DefaultDeviceId) {
                deviceId = getRecorderBackend().getDeviceList().getDefaultDeviceId();
            } else {
                deviceId = this.deviceId;
            }

            const state = await new Promise<audio.record.DeviceSetResult>(resolve => this.nativeHandle.set_device(deviceId, resolve));
            if(state !== "success") {
                if(state === "invalid-device") {
                    return InputStartError.EDEVICEUNKNOWN;
                } else if(state === undefined) {
                    throw tr("invalid set device result state");
                }

                logError(LogCategory.AUDIO, tr("Native audio driver returned invalid device set result: %o"), state);
                throw tr("unknown device change result");
            }

            await new Promise((resolve, reject) => this.nativeHandle.start(result => {
                if(result === true) {
                    resolve();
                } else {
                    reject(typeof result === "string" ? result : tr("failed to start input"));
                }
            }));

            this.setState(InputState.RECORDING);
            return true;
        } finally {
            /* @ts-ignore Typescript isn't smart about awaits in try catch blocks */
            if(this.state === InputState.INITIALIZING) {
                this.setState(InputState.PAUSED);
            }
        }
    }

    async stop(): Promise<void> {
        if(this.state === InputState.PAUSED) {
            return;
        }

        this.nativeHandle.stop();
        this.setState(InputState.PAUSED);

        if(this.filtered) {
            this.filtered = false;
            this.events.fire("notify_voice_end");
        }
    }

    async setDeviceId(device: string | undefined): Promise<void> {
        if(this.deviceId === device) {
            return;
        }

        try {
            await this.stop();
        } catch (error) {
            logWarn(LogCategory.GENERAL, tr("Failed to stop microphone recording after device change: %o"), error);
        }

        const oldDeviceId = this.deviceId;
        this.deviceId = device;
        this.events.fire("notify_device_changed", { oldDeviceId, newDeviceId: device });
    }

    currentDeviceId(): string | undefined {
        return this.deviceId;
    }

    isFiltered(): boolean {
        return this.filtered;
    }

    removeFilter(filter: Filter) {
        const index = this.registeredFilters.indexOf(filter as any);
        if(index === -1) {
            return;
        }

        const [ registeredFilter ] = this.registeredFilters.splice(index, 1);
        registeredFilter.finalize();
    }

    createFilter<T extends FilterType>(type: T, priority: number): FilterTypeClass<T> {
        let filter;
        switch (type) {
            case FilterType.STATE:
                filter = new NStateFilter(this, priority);
                break;

            case FilterType.THRESHOLD:
                filter = new NThresholdFilter(this, priority);
                break;

            case FilterType.VOICE_LEVEL:
                filter = new NVoiceLevelFilter(this, priority);
                break;
        }

        this.registeredFilters.push(filter);
        return filter;
    }

    supportsFilter(type: FilterType): boolean {
        switch (type) {
            case FilterType.VOICE_LEVEL:
            case FilterType.THRESHOLD:
            case FilterType.STATE:
                return true;

            default:
                return false;
        }
    }

    currentState(): InputState {
        return this.state;
    }

    currentConsumer(): InputConsumer | undefined {
        return {
            type: InputConsumerType.NATIVE
        };
    }

    getNativeConsumer() : audio.record.AudioConsumer {
        return this.nativeConsumer;
    }

    async setConsumer(consumer: InputConsumer): Promise<void> {
        if(typeof(consumer) !== "undefined") {
            throw tr("we only support native consumers!"); // TODO: May create a general wrapper?
        }

        return;
    }

    setVolume(volume: number) {
        this.nativeHandle.set_volume(volume);
    }

    getVolume(): number {
        return this.nativeHandle.get_volume();
    }

    getFilterMode(): FilterMode {
        const mode = this.nativeConsumer.get_filter_mode();
        switch (mode) {
            case NativeFilterMode.Block:
                return FilterMode.Block;

            case NativeFilterMode.Bypass:
                return FilterMode.Bypass;

            case NativeFilterMode.Filter:
            default:
                return FilterMode.Filter;
        }
    }

    setFilterMode(mode: FilterMode) {
        let nativeMode: NativeFilterMode;
        switch (mode) {
            case FilterMode.Filter:
                nativeMode = NativeFilterMode.Filter;
                break;

            case FilterMode.Bypass:
                nativeMode = NativeFilterMode.Bypass;
                break;

            case FilterMode.Block:
                nativeMode = NativeFilterMode.Block;
                break;
        }

        if(this.nativeConsumer.get_filter_mode() === nativeMode) {
            return;
        }

        const oldMode = this.getFilterMode();
        this.nativeConsumer.set_filter_mode(nativeMode);
        this.events.fire("notify_filter_mode_changed", { oldMode, newMode: mode });
    }

    getInputProcessor(): InputProcessor {
        return this.inputProcessor;
    }

    createLevelMeter(): LevelMeter {
        return new NativeInputLevelMeter(this.nativeHandle.create_level_meter("post-process"));
    }
}

class NativeInputProcessor implements InputProcessor {
    private readonly processor: AudioProcessor;

    constructor(processor: AudioProcessor) {
        this.processor = processor;
    }

    applyProcessorConfig<T extends InputProcessorType>(processor: T, config: Partial<InputProcessorConfigMapping[T]>) {
        let keys: string[];
        switch (processor) {
            case "webrtc-processing":
                keys = kInputProcessorConfigWebRTCKeys;
                break;

            case "rnnoise":
                keys = kInputProcessorConfigRNNoiseKeys;
                break;

            default:
                throw "invalid processor";
        }

        const filteredConfig = {};
        keys.forEach(key => {
            if(typeof config[key] === "undefined") {
                return;
            }

            filteredConfig[key] = config[key];
        });
        this.processor.apply_config(filteredConfig);
    }

    getProcessorConfig<T extends InputProcessorType>(processor: T): InputProcessorConfigMapping[T] {
        let keys: string[];
        const config = this.processor.get_config();

        switch (processor) {
            case "webrtc-processing":
                keys = kInputProcessorConfigWebRTCKeys;
                break;

            case "rnnoise":
                keys = kInputProcessorConfigRNNoiseKeys;
                break;

            default:
                throw "invalid processor";
        }

        const result = {};
        keys.forEach(key => result[key] = config[key]);
        return result as any;
    }

    getStatistics(): InputProcessorStatistics {
        return this.processor.get_statistics();
    }

    hasProcessor(processor: InputProcessorType): boolean {
        switch (processor) {
            case "rnnoise":
            case "webrtc-processing":
                return true;

            default:
                return false;
        }
    }
}

class NativeInputLevelMeter implements LevelMeter {
    private nativeHandle: audio.record.AudioLevelMeter;

    constructor(nativeHandle: audio.record.AudioLevelMeter) {
        this.nativeHandle = nativeHandle;
        this.nativeHandle.start(error => {
            if(typeof error !== "undefined") {
                logError(LogCategory.AUDIO, tr("Native input audio level meter failed to start. This should not happen. Reason: %o"), error);
            }
        });
    }

    destroy(): any {
        this.nativeHandle?.set_callback(undefined);
        this.nativeHandle?.stop();
        this.nativeHandle = undefined;
    }

    getDevice(): InputDevice {
        return undefined;
    }

    setObserver(callback: (value: number) => any) {
        this.nativeHandle.set_callback(level => {
            try {
                callback(level);
            } catch (error) {
                console.error(error);
            }
        });
    }
}

export class NativeLevelMeter implements LevelMeter {
    readonly targetDevice: InputDevice;

    private nativeHandle: audio.record.AudioLevelMeter;

    constructor(device: InputDevice) {
        this.targetDevice = device;
    }

    async initialize() {
        try {
            this.nativeHandle = audio.record.create_device_level_meter(this.targetDevice.deviceId);
            await new Promise((resolve, reject) => {
                this.nativeHandle.start(error => {
                    if(typeof error !== "undefined") {
                        reject(error);
                    } else {
                        resolve(error);
                    }
                });
            });

            /* TODO: May implement smoothing to the native level meter as well? */
            //this.nativeFilter.set_attack_smooth(.75);
            //this.nativeFilter.set_release_smooth(.75);
        } catch (error) {
            if (typeof (error) === "string") {
                throw error;
            }

            logWarn(LogCategory.AUDIO, tr("Failed to initialize level meter for device %o: %o"), this.targetDevice, error);
            throw "initialize failed (lookup console)";
        }
    }

    destroy() {
        this.nativeHandle?.set_callback(undefined);
        this.nativeHandle?.stop();
        this.nativeHandle = undefined;
    }

    getDevice(): InputDevice {
        return this.targetDevice;
    }

    setObserver(callback: (value: number) => void) {
        this.nativeHandle.set_callback(level => {
            try {
                callback(level);
            } catch (error) {
                console.error(error);
            }
        });
    }
}