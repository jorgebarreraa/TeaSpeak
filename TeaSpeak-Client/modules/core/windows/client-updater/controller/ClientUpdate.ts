import {BrowserWindow, dialog} from "electron";
import * as url from "url";
import * as path from "path";
import {loadWindowBounds, startTrackWindowBounds} from "../../../../shared/window";
import {hideAppLoaderWindow} from "../../app-loader/controller/AppLoader";
import {
    availableRemoteChannels, clientAppInfo, clientUpdateChannel,
    currentClientVersion,
    newestRemoteClientVersion, prepareUpdateExecute, setClientUpdateChannel,
    UpdateVersion
} from "../../../app-updater";
import {closeMainWindow} from "../../main-window/controller/MainWindow";

const kDeveloperTools = false;

let windowInstance: BrowserWindow;
let windowSpawnPromise: Promise<void>;

let currentRemoteUpdateVersion: UpdateVersion;

let updateInstallExecuteCallback;
let updateInstallAbortCallback;

export async function showUpdateWindow() {
    while(windowSpawnPromise) {
        await windowSpawnPromise;
    }

    if(windowInstance) {
        windowInstance.focus();
        return;
    }

    windowSpawnPromise = doSpawnWindow().catch(error => {
        console.error("Failed to open the client updater window: %o", error);
        dialog.showErrorBox("Failed to open window", "Failed to open the client updater window.\nLookup the console for details.");
        hideAppLoaderWindow();
    });
    /* do this after the assignment so in case the promise resolves instantly we still clear the assignment */
    windowSpawnPromise.then(() => windowSpawnPromise = undefined);

    await windowSpawnPromise;
    console.error("Window created");
}

const kZoomFactor = 1;
async function doSpawnWindow() {
    const kWindowWidth = kZoomFactor * 580 + (kDeveloperTools ? 1000 : 0);
    const kWindowHeight = kZoomFactor * 800 + (process.platform == "win32" ? 40 : 0);

    windowInstance = new BrowserWindow({
        width: kWindowWidth,
        height: kWindowHeight,
        frame: kDeveloperTools,
        resizable: kDeveloperTools,
        show: false,
        autoHideMenuBar: true,
        webPreferences: {
            nodeIntegration: true,
            zoomFactor: kZoomFactor
        }
    });

    fatalErrorHandled = false;
    targetRemoteVersion = undefined;
    currentRemoteUpdateVersion = undefined;

    windowInstance.setMenu(null);
    windowInstance.on('closed', () => {
        windowInstance = undefined;
        if(updateInstallAbortCallback) {
            /* cleanup */
            updateInstallAbortCallback();
        }
        updateInstallAbortCallback = undefined;
        updateInstallExecuteCallback = undefined;
    });

    if(kDeveloperTools) {
        windowInstance.webContents.openDevTools();
    }

    initializeIpc();

    await windowInstance.loadURL(url.pathToFileURL(path.join(__dirname, "..", "renderer", "index.html")).toString());
    windowInstance.show();

    try {
        await loadWindowBounds('client-updater', windowInstance, undefined, { applySize: false });
        startTrackWindowBounds('client-updater', windowInstance);
    } catch (error) {
        console.warn("Failed to load and track window bounds");
    }
}

let fatalErrorHandled = false;
async function handleFatalError(error: string, popupMessage?: string) {
    /* Show only one error at the time */
    if(fatalErrorHandled) { return; }
    fatalErrorHandled = true;

    windowInstance?.webContents.send("client-updater-set-error", error);

    await dialog.showMessageBox(windowInstance, {
        type: "error",
        buttons: ["Ok"],
        message: "A critical error happened:\n" + (popupMessage || error)
    });

    fatalErrorHandled = false;
}

async function sendLocalInfo() {
    try {
        const localVersion = await currentClientVersion();
        if(localVersion.isDevelopmentVersion()) {
            windowInstance?.webContents.send("client-updater-local-status", "InDev", Date.now());
        } else {
            windowInstance?.webContents.send("client-updater-local-status", localVersion.toString(false), localVersion.timestamp);
        }
    } catch (error) {
        console.error("Failed to query/send the local client version: %o", error);
        handleFatalError("Failed to query local version").then(undefined);
    }
}

let targetRemoteVersion: UpdateVersion;
function initializeIpc() {
    windowInstance.webContents.on("ipc-message", (event, channel, ...args) => {
        switch (channel) {
            case "client-updater-close":
                closeUpdateWindow();
                break;

            case "client-updater-query-local-info":
                sendLocalInfo().then(undefined);
                break;

            case "client-updater-query-remote-info":
                newestRemoteClientVersion(clientUpdateChannel()).then(async result => {
                    currentRemoteUpdateVersion = result;
                    if(!result) {
                        await handleFatalError("No remote update info.");
                        return;
                    }

                    const localVersion = await currentClientVersion();
                    const updateAvailable = !localVersion.isDevelopmentVersion() && (result.version.newerThan(localVersion, true) || result.channel !== clientAppInfo().clientChannel);
                    targetRemoteVersion = updateAvailable ? result : undefined;

                    windowInstance?.webContents.send("client-updater-remote-status",
                        updateAvailable,
                        result.version.toString(false),
                        result.version.timestamp
                    );

                }).catch(async error => {
                    currentRemoteUpdateVersion = undefined;
                    console.error("Failed to query remote client version: %o", error);
                    await handleFatalError("Failed to query server info.", typeof error === "string" ? error : undefined);
                });
                break;

            case "client-updater-query-channels":
                availableRemoteChannels().then(channels => {
                    windowInstance?.webContents.send("client-updater-channel-info", channels, clientUpdateChannel());
                }).catch(async error => {
                    console.error("Failed to query available channels %o", error);
                    await handleFatalError("Failed to query available channels.", typeof error === "string" ? error : undefined);
                });
                break;

            case "client-updater-set-channel":
                setClientUpdateChannel(args[0] || "release");
                break;

            case "execute-update":
                doExecuteUpdate();
                break;

            case "install-update":
                updateInstallExecuteCallback();
                break;

            default:
                /* nothing to do */
                break;
        }
    });
}

function doExecuteUpdate() {
    windowInstance?.webContents.send("client-updater-execute");

    if(!currentRemoteUpdateVersion) {
        windowInstance?.webContents.send("client-updater-execute-finish", "Missing target version");
        return;
    }

    closeMainWindow(true);
    prepareUpdateExecute(currentRemoteUpdateVersion, (message, progress) => {
        windowInstance?.webContents.send("client-updater-execute-progress", message, progress);
    }, (type, message) => {
        windowInstance?.webContents.send("client-updater-execute-log", type, message);
    }).then(callbacks => {
        updateInstallExecuteCallback = callbacks.callbackExecute;
        updateInstallAbortCallback = callbacks.callbackAbort;
        windowInstance?.webContents.send("client-updater-execute-finish");
    }).catch(error => {
        windowInstance?.webContents.send("client-updater-execute-finish", error);
    });
}

export function closeUpdateWindow() {
    (async () => {
        await windowSpawnPromise;
        if(windowInstance) {
            windowInstance.close();
            windowInstance = undefined;
        }
    })();
}