import {remote} from "electron";
import * as electron from "electron";
import * as os from "os";
import * as path from "path";

export function setup_require(module: NodeModule) {
    module.paths.push(native_module_path());
}

export function native_module_path() {
    const app_path = (remote || electron).app.getAppPath();

    if(!app_path.endsWith(".asar")) {
        if(os.platform() === "win32" && false) {
            const win64 = process.env.hasOwnProperty('ProgramFiles(x86)');
            return path.join(
                app_path,
                "native",
                "build",
                os.platform() + "_" + (win64 ? "x64" : "x86")
            );
        } else {
            return path.join(
                app_path,
                "native",
                "build",
                os.platform() + "_" + os.arch()
            );
        }
    } else {
        return path.join(path.dirname(app_path), "natives");
    }
}