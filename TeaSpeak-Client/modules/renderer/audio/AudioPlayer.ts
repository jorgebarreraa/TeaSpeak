import * as native from "tc-native/connection";
import {AudioBackend, OutputDevice} from "tc-shared/audio/Player";

export class NativeAudioPlayer implements AudioBackend {
    private readonly audioContext: AudioContext;
    private initializedPromises: (() => void)[];

    private currentDevice: native.audio.AudioDevice;

    constructor() {
        this.audioContext = new AudioContext();
        this.initializedPromises = [];

        native.audio.initialize(() => {
            const promises = this.initializedPromises;
            this.initializedPromises = undefined;

            promises.forEach(callback => callback());
        });
    }

    executeWhenInitialized(callback: () => void): any {
        if(this.initializedPromises) {
            this.initializedPromises.push(callback);
        } else {
            callback();
        }
    }

    getMasterVolume(): number {
        return native.audio.playback.get_master_volume();
    }

    setMasterVolume(volume: number): any {
        native.audio.playback.set_master_volume(volume);
    }

    getAudioContext(): AudioContext | undefined {
        return this.audioContext;
    }

    async getAvailableDevices(): Promise<OutputDevice[]> {
        const devices = native.audio.available_devices().filter(e => e.output_supported || e.output_default);
        /* Fixup MMEs character limit of 31 by trying to resolve a name somewhere else */
        devices.forEach(device => {
            if(device.driver !== "MME") {
                return;
            }

            const fallback = devices.find(fallbackDevice => fallbackDevice.name.substring(0, 31) === device.name && fallbackDevice.name.length > 31);
            if(fallback) {
                device.name = fallback.name;
            }
        });

        return devices.map(entry => ({
            deviceId: entry.device_id,
            driver: entry.driver,
            name: entry.name
        }));
    }

    getCurrentDevice(): OutputDevice {
        if(this.currentDevice) {
            return {
                name: this.currentDevice.name,
                driver: this.currentDevice.driver,
                deviceId: this.currentDevice.device_id
            };
        }

        const defaultDevice = native.audio.available_devices().find(entry => entry.output_default);
        if(defaultDevice) {
            return {
                name: defaultDevice.name,
                driver: defaultDevice.driver,
                deviceId: defaultDevice.device_id
            };
        }

        return {
            name: "Default device",
            deviceId: "default",
            driver: "default driver"
        };
    }

    async setCurrentDevice(targetId: string | undefined): Promise<void> {
        const device = native.audio.available_devices().find(e => e.device_id == targetId);
        if(!device) {
            console.warn("Missing audio device with is %s", targetId);
            throw "invalid device id";
        }

        try {
            native.audio.playback.set_device(device.device_id);
        } catch(error) {
            if(error instanceof Error) {
                throw error.message;
            }

            throw error;
        }

        this.currentDevice = device;
    }

    getDefaultDeviceId(): string {
        return native.audio.available_devices().find(entry => entry.output_default)?.device_id || "default";
    }

    isDeviceRefreshAvailable(): boolean {
        return false;
    }

    isInitialized(): boolean {
        return !this.initializedPromises;
    }

    refreshDevices(): Promise<void> {
        return Promise.resolve(undefined);
    }
}