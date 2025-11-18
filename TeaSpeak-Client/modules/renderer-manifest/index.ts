/* --------------- bootstrap  --------------- */
import * as RequireProxy from "../renderer/RequireProxy";
import * as path from "path";
RequireProxy.initialize(path.join(__dirname, "backend-impl"), "modal-external");

/* --------------- entry point  --------------- */
import * as loader from "tc-loader";
import {Stage} from "tc-loader";
import {Arguments, processArguments} from "../shared/process-arguments";
import {remote} from "electron";

export function initialize() {
    console.log("Initializing native client");

    const _impl = message => {
        if(!processArguments.has_flag(Arguments.DEBUG)) {
            console.error("Displaying critical error: %o", message);
            message = message.replace(/<br>/i, "\n");

            const win = remote.getCurrentWindow();
            win.webContents.openDevTools();

            remote.dialog.showMessageBox({
                type: "error",
                buttons: ["exit"],
                title: "A critical error happened!",
                message: message
            });

        } else {
            console.error("Received critical error: %o", message);
            console.error("Ignoring error due to the debug mode");
        }
    };

    if(window.impl_display_critical_error) {
        window.impl_display_critical_error = _impl;
    } else {
        window.displayCriticalError = _impl;
    }

    loader.register_task(Stage.JAVASCRIPT, {
        name: "handler initialize #2",
        priority: -1,
        function: async () => {
            await import("../renderer/hooks/StorageAdapter");
        }
    });
}