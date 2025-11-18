require("../shared/require").setup_require(module);
import {app, BrowserWindow, dialog, remote} from "electron";
import * as path from "path";
import * as electron from "electron";
import * as os from "os";
import * as url from "url";

export function handle_crash_callback(args: string[]) {
    const parameter = {};
    for(const argument of args) {
        const colon_index = argument.indexOf('=');
        if(colon_index == -1) {
            console.warn("Crash callback contains invalid argument! (%s)", argument);
            continue;
        }

        parameter[argument.substr(0, colon_index)] = argument.substr(colon_index + 1);
    }
    console.log("Received crash dump callback. Arguments: %o", parameter);

    let error;
    let crashFile;

    if(parameter["success"] == true) {
        /* okey we have an crash dump */
        crashFile = parameter["dump_path"];
        if(typeof(crashFile) === "string") {
            try {
                crashFile = Buffer.from(crashFile, 'base64').toString();
            } catch(error) {
                console.warn("Failed to decode dump path: %o", error);
                crashFile = undefined;
                error = "failed to decode dump path!";
            }
        }
    } else if(typeof(parameter["error"]) === "string") {
        try {
            error = Buffer.from(parameter["error"], 'base64').toString();
        } catch(error) {
            console.warn("Failed to decode error: %o", error);
            error = "failed to decode error";
        }
    } else {
        error = "missing parameters";
    }

    app.on('ready', () => {
        const crashWindow = new BrowserWindow({
            show: false,
            width: 1000,
            height: 300 + (os.platform() === "win32" ? 50 : 0),

            webPreferences: {
                devTools: true,
                nodeIntegration: true,
                javascript: true
            }
        });
        crashWindow.on('focus', event => crashWindow.flashFrame(false));

        crashWindow.setMenu(null);
        crashWindow.loadURL(url.pathToFileURL(path.join(path.dirname(module.filename), "ui", "index.html")).toString()).catch(error => {
            dialog.showErrorBox("Crash window failed to load", "Failed to load the crash window.\nThis indicates that something went incredible wrong.\n\nError:\n" + error);
        });

        crashWindow.on('ready-to-show', () => {
            if(error) {
                crashWindow.webContents.send('dump-error', error);
            } else if(!crashFile) {
                crashWindow.webContents.send('dump-error', "Missing crash file");
            } else {
                crashWindow.webContents.send('dump-url', crashFile);
            }

            crashWindow.show();
            crashWindow.setProgressBar(1, { mode: "error" });
            crashWindow.flashFrame(true);
        });

        app.on('window-all-closed', () => {
            process.exit(0);
        });
    });
    app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required');
}

export const handler = require("teaclient_crash_handler");
if(typeof window === "object") {
    (window as any).crash =  handler;
}

export function initialize_handler(component_name: string, requires_file: boolean) {
    const start_path = requires_file ? (" "  + path.join(__dirname, "..", "..")) : "";
    const success_arguments = process.argv[0] + start_path + " crash-handler success=1 dump_path=%crash_path%";
    const error_arguments = process.argv[0] + start_path + " crash-handler success=0 error=%error_message%";

    console.log("Setting up crash handler. Success callback: %s; Error callback: %s", success_arguments, error_arguments);
    handler.setup_crash_handler(
        component_name,
        path.join((remote || electron).app.getPath('userData'), "crash_dumps"),
        success_arguments,
        error_arguments
    );
}

export function finalize_handler() {
    handler.finalize();
}