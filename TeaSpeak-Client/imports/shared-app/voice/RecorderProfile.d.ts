import { AbstractInput } from "../voice/RecorderBase";
import { KeyDescriptor } from "../PPTListener";
import { ConnectionHandler } from "../ConnectionHandler";
import { InputDevice } from "../audio/Recorder";
import { Registry } from "tc-shared/events";
export declare type VadType = "threshold" | "push_to_talk" | "active";
export interface RecorderProfileConfig {
    version: number;
    device_id: string | undefined;
    volume: number;
    vad_type: VadType;
    vad_threshold: {
        threshold: number;
    };
    vad_push_to_talk: {
        delay: number;
        key_code: string;
        key_ctrl: boolean;
        key_windows: boolean;
        key_shift: boolean;
        key_alt: boolean;
    };
}
export interface DefaultRecorderEvents {
    notify_default_recorder_changed: {};
}
export declare let defaultRecorder: RecorderProfile;
export declare const defaultRecorderEvents: Registry<DefaultRecorderEvents>;
export declare function setDefaultRecorder(recorder: RecorderProfile): void;
export interface RecorderProfileEvents {
    notify_device_changed: {};
    notify_voice_start: {};
    notify_voice_end: {};
    notify_input_initialized: {};
}
export declare abstract class RecorderProfileOwner {
    /**
     * This method will be called from the recorder profile.
     */
    protected abstract handleUnmount(): any;
    /**
     * This callback will be called when the recorder audio input has
     * been initialized.
     * Note: This method might be called within ownRecorder().
     *       If this method has been called, handleUnmount will be called.
     *
     * @param input The target input.
     */
    protected abstract handleRecorderInput(input: AbstractInput): any;
}
export declare abstract class ConnectionRecorderProfileOwner extends RecorderProfileOwner {
    abstract getConnection(): ConnectionHandler;
}
export declare class RecorderProfile {
    readonly events: Registry<RecorderProfileEvents>;
    readonly name: any;
    readonly volatile: any;
    config: RecorderProfileConfig;
    input: AbstractInput;
    private currentOwner;
    private currentOwnerMutex;
    current_handler: ConnectionHandler;
    private readonly pptHook;
    private pptTimeout;
    private pptHookRegistered;
    private registeredFilter;
    constructor(name: string, volatile?: boolean);
    destroy(): void;
    initialize(): Promise<void>;
    private initializeInput;
    private save;
    private reinitializePPTHook;
    private reinitializeFilter;
    /**
     * Own the recorder.
     */
    ownRecorder(target: RecorderProfileOwner | undefined): Promise<void>;
    getOwner(): RecorderProfileOwner | undefined;
    isInputActive(): boolean;
    /** @deprecated use `ownRecorder(undefined)` */
    unmount(): Promise<void>;
    getVadType(): VadType;
    setVadType(type: VadType): boolean;
    getThresholdThreshold(): number;
    setThresholdThreshold(value: number): void;
    getPushToTalkKey(): KeyDescriptor;
    setPushToTalkKey(key: KeyDescriptor): void;
    getPushToTalkDelay(): number;
    setPushToTalkDelay(value: number): void;
    getDeviceId(): string | typeof InputDevice.DefaultDeviceId | typeof InputDevice.NoDeviceId;
    setDevice(device: InputDevice | typeof InputDevice.DefaultDeviceId | typeof InputDevice.NoDeviceId): Promise<void>;
    getVolume(): number;
    setVolume(volume: number): void;
}
