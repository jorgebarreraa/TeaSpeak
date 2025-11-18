import * as path from "path";
module.paths.push(path.join(__dirname, "..", "..", "..", "build", "win32_x64"));
module.paths.push(path.join(__dirname, "..", "..", "..", "build", "linux_x64"));

import {audio} from "teaclient_connection.node";
import record = audio.record;
import playback = audio.playback;

function printDevices() {
    console.info("Available input devices:");
    for(const device of audio.available_devices()) {
        if(!device.input_supported) {
            continue;
        }

        console.info(" - " + device.driver + " - " + device.device_id + " (" + device.name + ")" + (device.input_default ? " (default)" : ""));
    }

    console.info("Available output devices:");
    for(const device of audio.available_devices()) {
        if(!device.output_supported) {
            continue;
        }

        console.info(" - " + device.driver + " - " + device.device_id + " (" + device.name + ")" + (device.output_default ? " (default)" : ""));
    }
}

async function levelMeterDevice(deviceId: string) {
    const levelMeter = record.create_device_level_meter(deviceId);
    await new Promise((resolve, reject) => {
        levelMeter.start(error => {
            if(typeof error !== "undefined") {
                reject(error);
            } else {
                resolve(error);
            }
        });
    });

    console.info("Started");
    levelMeter.set_callback(level => console.info("New level: %o", level));
    await new Promise(resolve => setTimeout(resolve, 10 * 1000));

    console.info("Stop");
    levelMeter.stop();

    await new Promise(resolve => setTimeout(resolve, 1000));
    console.info("Done");
}

async function main() {
    await new Promise(resolve => audio.initialize(resolve));

    console.info("Audio initialized");
    //printDevices();

    const defaultInput = audio.available_devices().find(device => device.input_default);
    if(!defaultInput) {
        throw "missing default input device";
    }

    await levelMeterDevice(defaultInput.device_id);
    return;

    const recorder = record.create_recorder();
    await new Promise((resolve, reject) => {

        recorder.set_device(defaultInput.device_id, result => {
            if(result === "success") {
                resolve();
            } else {
                reject(result);
            }
        });
    });

    await new Promise((resolve, reject) => {
        recorder.start(result => {
            if(typeof result === "boolean" && result) {
                resolve();
            } else {
                reject(result);
            }
        });
    });

    const output = playback.create_stream();
    const recorderConsumer = recorder.create_consumer();

    if(output.channels !== recorderConsumer.channelCount) {
        throw "miss matching channel count";
    }

    if(output.sample_rate !== recorderConsumer.sampleRate) {
        throw "miss matching sample rate";
    }

    recorderConsumer.callback_data = buffer => output.write_data(buffer.buffer, true);

    setInterval(() => {
        const processor = recorder.get_audio_processor();
        if(!processor) { return; }

        console.error("Config:\n%o", processor.get_config());
        console.error("Statistics:\n%o", processor.get_statistics());
        processor.apply_config({
            "echo_canceller.enabled": false,
            "rnnoise.enabled": true
        });
    }, 2500);
}

main().catch(error => {
    console.error("An error occurred:");
    console.error(error);
    process.exit(1);
});

/*
handle.audio.initialize(() => {
    console.log("Audio initialized");

    console.log("Query devices...");
    console.log("Devices: %o", handle.audio.available_devices());
    console.log("Current playback device: %o", handle.audio.playback.current_device());

    const stream = handle.audio.playback.create_stream();
    console.log("Own stream: %o", stream);
    stream.set_buffer_latency(0.02);
    stream.set_buffer_max_latency(0.2);

    const recorder = handle.audio.record.create_recorder();
    const default_input = handle.audio.available_devices().find(e => e.input_default);
    console.log(default_input);
    console.log(handle.audio.available_devices().find(e => e.device_id == handle.audio.playback.current_device()));

    recorder.set_device(default_input.device_id, () => {
        const consumer = recorder.create_consumer();
        consumer.callback_data = buffer => {
            stream.write_data(buffer.buffer, true);
        };
        recorder.start(result => {
            console.log("Start result: %o", result);
        });
    });

    setInterval(() => {
        handle.audio.sounds.playback_sound({
            file: "D:\\TeaSpeak\\web\\shared\\audio\\speech\\connection.refused.wav",
            //file: "D:\\Users\\WolverinDEV\\Downloads\\LRMonoPhase4.wav",

            volume: 1,
            callback: (result, message) => {
                console.log("Result %s: %s", handle.audio.sounds.PlaybackResult[result], message);
            }
        });
    }, 500);

    /*
    setInterval(() => {
        const elements = handle.audio.available_devices().filter(e => e.input_supported);
        const dev = elements[Math.floor(Math.random() * elements.length)];
        recorder.set_device(dev.device_id, () => {
            console.log("Dev updated: %o", dev);

            recorder.start(() => {
                console.log("Started");
            });
        });
    }, 1000);

});
*/