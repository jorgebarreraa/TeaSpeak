import {AudioRecorderBacked, DeviceList, InputDevice, setRecorderBackend} from "tc-shared/audio/Recorder";
import {AbstractInput, LevelMeter} from "tc-shared/voice/RecorderBase";
import {inputDeviceList} from "../audio/InputDeviceList";
import {NativeInput, NativeLevelMeter} from "../audio/AudioRecorder";

setRecorderBackend(new class implements AudioRecorderBacked {
    createInput(): AbstractInput {
        return new NativeInput();
    }

    async createLevelMeter(device: InputDevice): Promise<LevelMeter> {
        const meter = new NativeLevelMeter(device);
        await meter.initialize();
        return meter;
    }

    getDeviceList(): DeviceList {
        return inputDeviceList;
    }
});