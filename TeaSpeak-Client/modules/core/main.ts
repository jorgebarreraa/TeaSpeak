import * as electron from "electron";

import {app, Menu} from "electron";
import MessageBoxOptions = electron.MessageBoxOptions;

import {processArguments, parseProcessArguments, Arguments} from "../shared/process-arguments";
import {openChangeLog as openChangeLog} from "./app-updater/changelog";
import * as crash_handler from "../crash_handler";
import {initializeSingleInstance} from "./MultiInstanceHandler";

import "./AppInstance";
import {dereferenceApp, referenceApp} from "./AppInstance";
import {showUpdateWindow} from "./windows/client-updater/controller/ClientUpdate";
import {initializeCustomUiPackProtocol} from "./ui-loader/Loader";

async function handleAppReady() {
    Menu.setApplicationMenu(null);

    if(processArguments.has_value("update-execute")) {
        console.log("Showing update window");
        await showUpdateWindow();
        return;
    } else if(processArguments.has_value("update-failed-new") || processArguments.has_value("update-succeed-new")) {
        const success = processArguments.has_value("update-succeed-new");
        let data: {
            parse_success: boolean;
            log_file?: string;
            error_id?: string;
            error_message?: string;
        } = {
            parse_success: false
        };
        try {
            let encoded_data = Buffer.from(processArguments.value("update-failed-new") || processArguments.value("update-succeed-new"), "base64").toString();
            for(const part of encoded_data.split(";")) {
                const index = part.indexOf(':');
                if(index == -1)
                    data[part] = true;
                else {
                    data[part.substr(0, index)] = Buffer.from(part.substr(index + 1), "base64").toString();
                }
            }
            data.parse_success = true;
        } catch(error) {
            console.warn("Failed to parse update response data: %o", error);
        }
        console.log("Update success: %o. Update data: %o", success, data);

        const isRootExec = process.getuid && process.getuid() === 0;

        let title;
        let type;
        let message;

        const buttons: ({
            key: string,
            callback: () => Promise<boolean>
        })[] = [];

        if(success) {
            openChangeLog();

            type = "info";
            title = "Update succeeded!";

            message = "Update has been successfully installed!\nWhat do you want to do next?";

            if(!isRootExec) {
                /* Don't start the app automatically if we're still having root privileges */
                buttons.push({
                    key: "Launch client",
                    callback: async () => false
                });
            }

            if(data.parse_success && data.log_file) {
                buttons.push({
                    key: "Open update log",
                    callback: async () => {
                        electron.shell.openItem(data.log_file);
                        return true;
                    }
                });
            }
        } else {
            type = "error";
            title = "Update failed!";

            message = "Failed to install update.";
            if(data.parse_success) {
                message += "\n\n";
                message += "Error ID:      " + (data.error_id || "undefined") + "\n";
                message += "Error Message: " + (data.error_message || "undefined") + "\n";
                message += "Installer log: " + (data.log_file || "undefined");
            } else {
                message += "\nUnknown error! Lookup the console for more details.";
            }

            if(!isRootExec) {
                /* Don't start the app automatically if we're still having root privileges */
                buttons.push({
                    key: "Ignore",
                    callback: async () => false
                });
                buttons.push({
                    key: "Retry update",
                    callback: async () => {
                        await showUpdateWindow();
                        return true;
                    }
                });
            }

            if(data.parse_success && data.log_file) {
                buttons.push({
                    key: "Open update log",
                    callback: async () => {
                        electron.shell.openItem(data.log_file);
                        return true;
                    }
                });
            }
        }
        buttons.push({
            key: "Close",
            callback: async () => true
        });

        referenceApp();
        try {
            const result = await electron.dialog.showMessageBox(null, {
                type: type,
                message: message,
                title: title,
                buttons: buttons.map(e => e.key)
            } as MessageBoxOptions);
            if(buttons[result.response].callback) {
                if(await buttons[result.response].callback()) {
                    return;
                }
            }
        } finally {
            dereferenceApp();
        }
    }

    /* register client a teaclient:// handler */
    if(app.getAppPath().endsWith(".asar")) {
        if(!app.setAsDefaultProtocolClient("teaclient", app.getPath("exe"))) {
            console.error("Failed to setup default teaclient protocol handler");
        }
    }

    referenceApp();
    try {
        const main = require("./main-window");
        await main.execute();
    } catch (error) {
        /* Reference will leak but we call exit manually */
        referenceApp();

        console.error(error);
        await electron.dialog.showMessageBox({
            type: "error",
            message: "Failed to execute app main!\n" + error,
            title: "Main execution failed!",
            buttons: ["close"]
        } as MessageBoxOptions);
        electron.app.exit(1);
    } finally {
        dereferenceApp();
    }
}

function main() {
    if('allowRendererProcessReuse' in app) {
        app.allowRendererProcessReuse = false;
    }

    parseProcessArguments();
    if(processArguments.has_value(Arguments.DISABLE_HARDWARE_ACCELERATION)) {
        app.disableHardwareAcceleration();
    }

    if(processArguments.has_value(Arguments.DUMMY_CRASH_MAIN)) {
        crash_handler.handler.crash();
    }

    if(!processArguments.has_value(Arguments.DEBUG) && !processArguments.has_value(Arguments.NO_SINGLE_INSTANCE)) {
        if(!initializeSingleInstance()) {
            console.log("Another instance is already running. Closing this instance");
            app.exit(0);
        }
    }

    initializeCustomUiPackProtocol();
    app.on('ready', handleAppReady);
}
export const execute = main;