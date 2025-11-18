export interface OutputDevice {
    deviceId: string;
    driver: string;
    name: string;
}
export declare namespace OutputDevice {
    const NoDeviceId = "none";
    const DefaultDeviceId = "default";
}
export interface AudioBackendEvents {
    notify_initialized: {};
    notify_volume_changed: {
        oldVolume: number;
        newVolume: number;
    };
}
export interface AudioBackend {
    isInitialized(): boolean;
    getAudioContext(): AudioContext | undefined;
    isDeviceRefreshAvailable(): boolean;
    refreshDevices(): Promise<void>;
    getAvailableDevices(): Promise<OutputDevice[]>;
    getDefaultDeviceId(): string;
    getCurrentDevice(): OutputDevice;
    setCurrentDevice(targetId: string | undefined): Promise<void>;
    getMasterVolume(): number;
    setMasterVolume(volume: number): any;
    executeWhenInitialized(callback: () => void): any;
}
export declare function getAudioBackend(): AudioBackend;
export declare function setAudioBackend(newBackend: AudioBackend): void;
