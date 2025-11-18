import { app } from "electron";
import {getMainWindow} from "./windows/main-window/controller/MainWindow";

export function handleSecondInstanceCall(argv: string[], _workingDirectory: string) {
    const original_args = argv.slice(1).filter(e => !e.startsWith("--original-process-start-time=") && e != "--allow-file-access-from-files");
    console.log("Second instance: %o", original_args);

    const mainWindow = getMainWindow()
    if(!mainWindow) {
        console.warn("Ignoring second instance call because we haven't yet started");
        return;
    }

    mainWindow.focus();
    execute_connect_urls(original_args);
}

export function execute_connect_urls(argv: string[]) {
    const connectUrls = argv.filter(e => e.startsWith("teaclient://"));
    for(const url of connectUrls) {
        console.log("Received connect url: %s", url);
        getMainWindow().webContents.send('connect', url);
    }
}

export function initializeSingleInstance() : boolean {
    if(!app.requestSingleInstanceLock()) {
        return false;
    }

    app.on('second-instance', (event, argv, workingDirectory) => handleSecondInstanceCall(argv, workingDirectory));
    return true;
}