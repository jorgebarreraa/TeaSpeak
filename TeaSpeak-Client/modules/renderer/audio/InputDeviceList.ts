import {AbstractDeviceList, DeviceListEvents, InputDevice, PermissionState} from "tc-shared/audio/Recorder";
import {Registry} from "tc-events";
import * as loader from "tc-loader";

import * as native from "tc-native/connection";
import {audio} from "tc-native/connection";
import {LogCategory, logTrace} from "tc-shared/log";

interface NativeIDevice extends InputDevice {
    isDefault: boolean
}

class InputDeviceList extends AbstractDeviceList {
    private cachedDevices: NativeIDevice[];

    constructor() {
        super();

        this.setPermissionState("granted");
    }

    isRefreshAvailable(): boolean {
        return false;
    }

    async refresh(): Promise<void> {
        throw "not supported";
    }

    async requestPermissions(): Promise<PermissionState> {
        return "granted";
    }

    getDefaultDeviceId(): string {
        return this.getDevices().find(e => e.isDefault)?.deviceId || "default";
    }

    getDevices(): NativeIDevice[] {
        if(this.cachedDevices) {
            return this.cachedDevices;
        }

        const nativeDeviceList = audio.available_devices();
        logTrace(LogCategory.AUDIO, tr("Native device list: %o"), nativeDeviceList);
        this.cachedDevices = nativeDeviceList
            .filter(e => e.input_supported || e.input_default)
            /* If we're using WDM-KS and opening the microphone view, for some reason the channels get blocked an never release.... */
            .filter(e => e.driver !== "Windows WDM-KS")
            .map(device => {
                return {
                    deviceId: device.device_id,
                    name: device.name,
                    driver: device.driver,
                    isDefault: device.input_default
                }
            });

        /* Fixup MMEs character limit of 31 by trying to resolve a name somewhere else */
        this.cachedDevices.forEach(device => {
            if(device.driver !== "MME") {
                return;
            }

            const fallback = this.cachedDevices.find(fallbackDevice => fallbackDevice.name.substring(0, 31) === device.name && fallbackDevice.name.length > 31);
            if(fallback) {
                device.name = fallback.name;
            }
        });

        this.setState("healthy");
        return this.cachedDevices;
    }

    getEvents(): Registry<DeviceListEvents> {
        return this.events;
    }
}

export let inputDeviceList;

loader.register_task(loader.Stage.JAVASCRIPT_INITIALIZING, {
    function: async () => {
        inputDeviceList = new InputDeviceList();
        await new Promise(resolve => {
            native.audio.initialize(() => {
                inputDeviceList.getDevices();
                resolve();
            });
        });
    },
    priority: 80,
    name: "initialize input devices"
});
