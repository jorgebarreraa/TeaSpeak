import { AbstractInput, LevelMeter } from "../voice/RecorderBase";
import { Registry } from "../events";
export interface AudioRecorderBacked {
    createInput(): AbstractInput;
    createLevelMeter(device: InputDevice): Promise<LevelMeter>;
    getDeviceList(): DeviceList;
}
export interface DeviceListEvents {
    notify_list_updated: {
        removedDeviceCount: number;
        addedDeviceCount: number;
    };
    notify_state_changed: {
        oldState: DeviceListState;
        newState: DeviceListState;
    };
    notify_permissions_changed: {
        oldState: PermissionState;
        newState: PermissionState;
    };
}
export declare type DeviceListState = "healthy" | "uninitialized" | "no-permissions" | "error";
export interface InputDevice {
    deviceId: string;
    driver: string;
    name: string;
}
export declare namespace InputDevice {
    const NoDeviceId = "none";
    const DefaultDeviceId = "default";
}
export declare type PermissionState = "granted" | "denied" | "unknown";
export interface DeviceList {
    getEvents(): Registry<DeviceListEvents>;
    isRefreshAvailable(): boolean;
    refresh(): Promise<void>;
    requestPermissions(): Promise<PermissionState>;
    getPermissionState(): PermissionState;
    getStatus(): DeviceListState;
    getDevices(): InputDevice[];
    getDefaultDeviceId(): string;
    awaitHealthy(): Promise<void>;
    awaitInitialized(): Promise<void>;
}
export declare abstract class AbstractDeviceList implements DeviceList {
    protected readonly events: Registry<DeviceListEvents>;
    protected listState: DeviceListState;
    protected permissionState: PermissionState;
    protected constructor();
    getStatus(): DeviceListState;
    getPermissionState(): PermissionState;
    protected setState(state: DeviceListState): void;
    protected setPermissionState(state: PermissionState): void;
    awaitInitialized(): Promise<void>;
    awaitHealthy(): Promise<void>;
    abstract getDefaultDeviceId(): string;
    abstract getDevices(): InputDevice[];
    abstract getEvents(): Registry<DeviceListEvents>;
    abstract isRefreshAvailable(): boolean;
    abstract refresh(): Promise<void>;
    abstract requestPermissions(): Promise<PermissionState>;
}
export declare function getRecorderBackend(): AudioRecorderBacked;
export declare function setRecorderBackend(instance: AudioRecorderBacked): void;
