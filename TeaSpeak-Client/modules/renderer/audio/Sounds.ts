import {audio as naudio} from "tc-native/connection";
import {SoundBackend, SoundFile} from "tc-shared/audio/Sounds";
import * as paths from "path";
import * as loader from "tc-loader";
import {Stage} from "tc-loader";
import {ipcRenderer} from "electron";
import {LogCategory, logDebug, logInfo} from "tc-shared/log";

let uiFilePath;
export class NativeSoundBackend implements SoundBackend {
    playSound(sound: SoundFile): Promise<void> {
        return new Promise((resolve, reject) => {
            if(!uiFilePath) {
                reject("Mussing UI file path");
                return;
            }

            const path = paths.join(uiFilePath, sound.path);
            console.log("replaying %s (volume: %f) from %s", sound.path, sound.volume, path);
            naudio.sounds.playback_sound({
                callback: (result, message) => {
                    if(result == naudio.sounds.PlaybackResult.SUCCEEDED) {
                        resolve();
                    } else {
                        reject(naudio.sounds.PlaybackResult[result].toLowerCase() + ": " + message);
                    }
                },
                file: path,
                volume: typeof sound.volume === "number" ? sound.volume : 1
            });
        });
    }
}

//tc-get-ui-path
loader.register_task(Stage.JAVASCRIPT_INITIALIZING, {
    name: "sound initialize",
    priority: 50,
    function: async () => {
        uiFilePath = await ipcRenderer.invoke("tc-get-ui-path");
        if(!uiFilePath) {
            logInfo(LogCategory.AUDIO, tr("Missing UI path. App sounds will not work."));
        } else {
            if(uiFilePath[0] === '/' && uiFilePath[2] === ':') {
                //e.g.: /C:/test...
                uiFilePath = uiFilePath.substr(1);
            }

            logDebug(LogCategory.AUDIO, tr("Received UI path: %s"), uiFilePath);
        }
    }
})