import {audio} from "tc-native/connection";
import {NativeInput} from "./AudioRecorder";
import {FilterType, StateFilter, ThresholdFilter, VoiceLevelFilter} from "tc-shared/voice/Filter";

export abstract class NativeFilter {
    readonly priority: number;

    handle: NativeInput;
    enabled: boolean = false;

    protected constructor(handle, priority: number) {
        this.handle = handle;
        this.priority = priority;
    }

    abstract initialize();
    abstract finalize();

    isEnabled(): boolean {
        return this.enabled;
    }

    setEnabled(flag: boolean): void {
        if(this.enabled === flag)
            return;

        this.enabled = flag;

        if(this.enabled) {
            this.initialize();
        } else {
            this.finalize();
        }
    }
}

export class NThresholdFilter extends NativeFilter implements ThresholdFilter {
    static readonly frames_per_second = 1 / (960 / 48000);

    readonly type: FilterType.THRESHOLD;
    private filter: audio.record.ThresholdConsumeFilter;

    private marginFrames: number = 25; /* 120ms */
    private threshold: number = 50;
    private callbackLevel: () => void;

    private _attack_smooth = 0;
    private _release_smooth = 0;

    private levelCallbacks: ((level: number) => void)[] = [];

    constructor(handle, priority: number) {
        super(handle, priority);

        Object.defineProperty(this, 'callback_level', {
            get(): any {
                return this.callbackLevel;
            }, set(v: any): void {
                if(v === this.callbackLevel)
                    return;

                this.callbackLevel = v;
                if(this.filter) {
                    this.filter.set_analyze_filter(v);
                }
            },
            enumerable: true,
            configurable: false,
        })
    }

    getMarginFrames(): number {
        return this.filter ? this.filter.get_margin_time() * NThresholdFilter.frames_per_second : this.marginFrames;
    }

    getThreshold(): number {
        return this.filter ? this.filter.get_threshold() : this.threshold;
    }

    setMarginFrames(value: number) {
        this.marginFrames = value;
        if(this.filter) {
            this.filter.set_margin_time(value / 960 / 1000);
        }
    }

    getAttackSmooth(): number {
        return this.filter ? this.filter.get_attack_smooth() : this._attack_smooth;
    }

    getReleaseSmooth(): number {
        return this.filter ? this.filter.get_release_smooth() : this._release_smooth;
    }

    setAttackSmooth(value: number) {
        this._attack_smooth = value;
        if(this.filter) {
            this.filter.set_attack_smooth(value);
        }
    }

    setReleaseSmooth(value: number) {
        this._release_smooth = value;
        if(this.filter) {
            this.filter.set_release_smooth(value);
        }
    }

    setThreshold(value: number): Promise<void> {
        if(typeof(value) === "string")
            value = parseInt(value); /* yes... this happens */
        this.threshold = value;
        if(this.filter)
            this.filter.set_threshold(value);
        return Promise.resolve();
    }

    finalize() {
        if(this.filter) {
            if(this.handle.getNativeConsumer()) {
                this.handle.getNativeConsumer().unregister_filter(this.filter);
            }

            this.filter = undefined;
        }
    }

    initialize() {
        const consumer = this.handle.getNativeConsumer();
        if(!consumer) {
            return;
        }

        this.finalize();
        this.filter = consumer.create_filter_threshold(this.threshold);
        this.filter.set_margin_time(this.marginFrames / NThresholdFilter.frames_per_second);
        this.filter.set_attack_smooth(this._attack_smooth);
        this.filter.set_release_smooth(this._release_smooth);
        this.updateAnalyzeFilterCallback();
    }

    registerLevelCallback(callback: (value: number) => void) {
        this.levelCallbacks.push(callback);
        this.updateAnalyzeFilterCallback();
    }

    removeLevelCallback(callback: (value: number) => void) {
        const index = this.levelCallbacks.indexOf(callback);
        if(index === -1) {
            return;
        }

        this.levelCallbacks.splice(index, 1);
        this.updateAnalyzeFilterCallback();
    }

    private updateAnalyzeFilterCallback() {
        if(this.levelCallbacks.length > 0) {
            this.filter.set_analyze_filter(value => this.levelCallbacks.forEach(callback => callback(value)));
        } else {
            this.filter.set_analyze_filter(undefined);
        }
    }
}

export class NStateFilter extends NativeFilter implements StateFilter {
    readonly type: FilterType.STATE;
    private filter: audio.record.StateConsumeFilter;
    private active = false;

    constructor(handle, priority: number) {
        super(handle, priority);
    }

    finalize() {
        if(this.filter) {
            const consumer = this.handle.getNativeConsumer();
            consumer?.unregister_filter(this.filter);
            this.filter = undefined;
        }
    }

    initialize() {
        const consumer = this.handle.getNativeConsumer();
        if(!consumer)
            return;

        this.finalize();
        this.filter = consumer.create_filter_state();
        this.filter.set_consuming(this.active);
    }

    isActive(): boolean {
        return this.active;
    }

    setState(state: boolean) {
        if(this.active === state)
            return;
        this.active = state;
        if(this.filter) {
            this.filter.set_consuming(state);
        }
    }
}

export class NVoiceLevelFilter extends NativeFilter implements VoiceLevelFilter {
    static readonly frames_per_second = 1 / (960 / 48000);

    readonly type: FilterType.VOICE_LEVEL;
    private filter: audio.record.VADConsumeFilter;
    private level = 3;
    private _margin_frames = 6;

    constructor(handle, priority: number) {
        super(handle, priority);
    }

    finalize() {
        if(this.filter) {
            const consumer = this.handle.getNativeConsumer();
            consumer?.unregister_filter(this.filter);
            this.filter = undefined;
        }
    }

    initialize() {
        const consumer = this.handle.getNativeConsumer();
        if(!consumer)
            return;

        this.finalize();
        this.filter = consumer.create_filter_vad(this.level);
        this.filter.set_margin_time(this._margin_frames / NVoiceLevelFilter.frames_per_second);
    }

    getLevel(): number {
        return this.level;
    }

    setLevel(value: number) {
        if(this.level === value)
            return;

        this.level = value;
        if(this.filter) {
            this.finalize();
            this.initialize();
        }
    }

    setMarginFrames(value: number) {
        this._margin_frames = value;
        if(this.filter)
            this.filter.set_margin_time(value / NVoiceLevelFilter.frames_per_second);
    }

    getMarginFrames(): number {
        return this.filter ? this.filter.get_margin_time() * NVoiceLevelFilter.frames_per_second : this._margin_frames;
    }
}