import {app, BrowserWindow, dialog} from "electron";
import {dereferenceApp, referenceApp} from "../../../AppInstance";
import {closeURLPreview, openURLPreview} from "../../../url-preview";
import {loadWindowBounds, startTrackWindowBounds} from "../../../../shared/window";
import {Arguments, processArguments} from "../../../../shared/process-arguments";
import {allow_dev_tools} from "../../../main-window";
import * as path from "path";

let windowInstance: BrowserWindow;

export async function showMainWindow(entryPointUrl: string) {
    if(windowInstance) {
        throw "main window already initialized";
    }

    // Create the browser window.
    console.log("Spawning main window");

    referenceApp(); /* main browser window references the app */
    windowInstance = new BrowserWindow({
        width: 800,
        height: 600,

        minHeight: 600,
        minWidth: 600,

        show: false,
        webPreferences: {
            webSecurity: false,
            nodeIntegrationInWorker: true,
            nodeIntegration: true,
            preload: path.join(__dirname, "..", "renderer", "PreloadScript.js"),
        },
        icon: path.join(__dirname, "..", "..", "..", "..", "resources", "logo.ico"),
    });

    windowInstance.webContents.on("certificate-error", (event, url, error, certificate, callback) => {
        console.log("Allowing untrusted certificate for %o", url);
        event.preventDefault();
        callback(true);
    });

    windowInstance.on('closed', () => {
        windowInstance = undefined;

        app.releaseSingleInstanceLock();
        closeURLPreview().then(undefined);
        dereferenceApp();
    });

    windowInstance.webContents.on('new-window', (event, urlString, frameName, disposition, options, additionalFeatures) => {
        if(frameName.startsWith("__modal_external__")) {
            return;
        }

        event.preventDefault();
        try {
            let url: URL;
            try {
                url = new URL(urlString);
            } catch(error) {
                throw "failed to parse URL";
            }

            {
                let protocol = url.protocol.endsWith(":") ? url.protocol.substring(0, url.protocol.length - 1) : url.protocol;
                if(protocol !== "https" && protocol !== "http") {
                    throw "invalid protocol (" + protocol + "). HTTP(S) are only supported!";
                }
            }

            openURLPreview(urlString).then(() => {});
        } catch(error) {
            console.error("Failed to open preview window for URL %s: %o", urlString, error);
            dialog.showErrorBox("Failed to open preview", "Failed to open preview URL: " + urlString + "\nError: " + error);
        }
    });

    windowInstance.webContents.on('crashed', () => {
        console.error("UI thread crashed! Closing app!");

        if(!processArguments.has_flag(Arguments.DEBUG)) {
            windowInstance.close();
        }
    });

    try {
        await windowInstance.loadURL(entryPointUrl);
    } catch (error) {
        console.error("Failed to load UI entry point (%s): %o", entryPointUrl, error);
        throw "failed to load entry point";
    }

    windowInstance.show();

    loadWindowBounds('main-window', windowInstance).then(() => {
        startTrackWindowBounds('main-window', windowInstance);

        windowInstance.focus();
        if(allow_dev_tools && !windowInstance.webContents.isDevToolsOpened()) {
            windowInstance.webContents.openDevTools();
        }
    });
}

export function closeMainWindow(force: boolean) {
    windowInstance?.close();
    if(force) {
        windowInstance?.destroy();
    }
}

export function getMainWindow() : BrowserWindow {
    return windowInstance;
}