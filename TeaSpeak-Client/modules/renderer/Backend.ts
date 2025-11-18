import {ipcRenderer, remote} from "electron";
import * as os from "os";
import {NativeClientBackend, NativeClientVersionInfo} from "tc-shared/backend/NativeClient";
import {Arguments, processArguments} from "../shared/process-arguments";

const call_basic_action = (name: string, ...args: any[]) => ipcRenderer.send('basic-action', name, ...args);
let versionInfo: NativeClientVersionInfo;
export class NativeClientBackendImpl implements NativeClientBackend {
    openChangeLog(): void {
        call_basic_action("open-changelog");
    }

    openClientUpdater(): void {
        call_basic_action("check-native-update");
    }

    openDeveloperTools(): void {
        call_basic_action("open-dev-tools");
    }

    quit(): void {
        call_basic_action("quit");
    }

    reloadWindow(): void {
        call_basic_action("reload-window")
    }

    showDeveloperOptions(): boolean {
        return processArguments.has_flag(Arguments.DEV_TOOLS);
    }

    getVersionInfo(): NativeClientVersionInfo {
        if(!versionInfo) {
            versionInfo = {
                version: remote.getGlobal("app_version_client") || "?.?.?",

                os_platform: os.platform(),
                os_platform_version: os.release(),

                os_architecture: os.arch()
            };
        }

        return versionInfo;
    }
}