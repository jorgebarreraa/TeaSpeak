import {loadWindowBounds, startTrackWindowBounds} from "../../../../shared/window";
import {BrowserWindow, dialog} from "electron";
import * as path from "path";
import * as url from "url";
import { screen } from "electron";

let kDeveloperTools = false;

let windowInstance: BrowserWindow;
let windowSpawnPromise: Promise<void>;

let currentStatus: string;
let currentProgress: number;

export async function showAppLoaderWindow() {
    while(windowSpawnPromise) {
        await windowSpawnPromise;
    }

    if(windowInstance) {
        console.error("Just focus");
        windowInstance.focus();
        return;
    }

    windowSpawnPromise = spawnAppLoaderWindow().catch(error => {
        console.error("Failed to open the app loader window: %o", error);
        dialog.showErrorBox("Failed to open window", "Failed to open the app loader window.\nLookup the console for details.");
        hideAppLoaderWindow();
    });
    /* do this after the assignment so in case the promise resolves instantly we still clear the assignment */
    windowSpawnPromise.then(() => windowSpawnPromise = undefined);

    await windowSpawnPromise;
}

export function getLoaderWindow() : BrowserWindow {
    return windowInstance;
}

async function spawnAppLoaderWindow() {
    console.debug("Opening app loader window.");

    const kWindowWidth = 340 + (kDeveloperTools ? 1000 : 0);
    const kWindowHeight = 400 + (process.platform == "win32" ? 40 : 0);

    windowInstance = new BrowserWindow({
        width: kWindowWidth,
        height: kWindowHeight,
        frame: kDeveloperTools,
        resizable: kDeveloperTools,
        show: false,
        autoHideMenuBar: true,
        webPreferences: {
            nodeIntegration: true,
        }
    });

    windowInstance.setMenu(null);
    windowInstance.on('closed', () => {
        windowInstance = undefined;
    });

    if(kDeveloperTools) {
        windowInstance.webContents.openDevTools();
    }

    await windowInstance.loadURL(url.pathToFileURL(path.join(__dirname, "..", "renderer", "index.html")).toString());
    setAppLoaderStatus(currentStatus, currentProgress);
    windowInstance.show();

    try {
        let bounds = screen.getPrimaryDisplay()?.bounds;
        let x, y;
        if(bounds) {
            x = (bounds.x | 0) + ((bounds.width | 0) - kWindowWidth) / 2;
            y = (bounds.y | 0) + ((bounds.height | 0) - kWindowHeight) / 2;
        } else {
            x = 0;
            y = 0;
        }

        console.log("Setting app loader ui position to %ox%o", x, y);
        if(typeof x === "number" && typeof y === "number") {
            windowInstance.setPosition(x, y);
        }
    } catch (error) {
        console.warn("Failed to apply app loader ui position: %o", error);
    }

    try {
        await loadWindowBounds('ui-load-window', windowInstance, undefined, { applySize: false });
        startTrackWindowBounds('ui-load-window', windowInstance);
    } catch (error) {
        console.warn("Failed to load and track window bounds: %o", error);
    }
}

export function hideAppLoaderWindow() {
    (async () => {
        await windowSpawnPromise;
        if(windowInstance) {
            windowInstance.close();
            windowInstance = undefined;
        }
    })();
}

export function setAppLoaderStatus(status: string, progress: number) {
    currentStatus = status;
    currentProgress = progress;

    windowInstance?.webContents.send("progress-update", status, progress);
}