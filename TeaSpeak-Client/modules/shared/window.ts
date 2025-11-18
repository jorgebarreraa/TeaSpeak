import * as electron from "electron";
import * as fs from "fs-extra";
import * as path from "path";

/* We read/write to this file every time again because this file could be used by multiple processes */
const configFile: string = path.join((electron.app || electron.remote.app).getPath('userData'), "window-bounds.json");

import BrowserWindow = Electron.BrowserWindow;
import Rectangle = Electron.Rectangle;

let changedData: {[key: string]: Rectangle} = {};
let changedDataSaveTimeout: number;

export async function saveChanges() {
    clearTimeout(changedDataSaveTimeout);

    try {
        const data = (await fs.pathExists(configFile) ? await fs.readJson(configFile) : {}) || {};
        Object.assign(data, changedData);
        changedData = {};

        await fs.ensureFile(configFile);
        await fs.writeJson(configFile, data);
        configFileExists = true;

        console.log("Window bounds have been successfully saved!");
    } catch(error) {
        console.warn("Failed to save window bounds: %o", error);
    }
}

let configFileExists: boolean;
export async function loadLastWindowsBounds(key: string) : Promise<Rectangle | undefined> {
    try {
        if(typeof configFileExists !== "boolean") {
            configFileExists = await fs.pathExists(configFile);
        }

        if(!configFileExists) {
            return undefined;
        }

        const data = await fs.readJson(configFile) || {};
        if(typeof data[key] === "object") {
            return data[key];
        }
    } catch(error) {
        console.warn("Failed to load window bounds for %s: %o", key, error);
    }

    return undefined;
}

export function startTrackWindowBounds(windowId: string, window: BrowserWindow) {
    const events = ['move', 'moved', 'resize'];

    const onWindowBoundsChanged = () => {
        changedData[windowId] = window.getBounds();
        if(window.isMaximized()) {
            changedData[windowId].width = -1;
            changedData[windowId].height = -1;
        }

        clearTimeout(changedDataSaveTimeout);
        changedDataSaveTimeout = setTimeout(saveChanges, 1000) as any;
    };

    for(const event of events) {
        window.on(event as any, onWindowBoundsChanged);
    }

    window.on('closed', () => {
        for(const event of events) {
            window.removeListener(event as any, onWindowBoundsChanged);
        }
    });
}

export async function loadWindowBounds(windowId: string, window: BrowserWindow, targetBounds?: Rectangle, options?: { applySize?: boolean; applyPosition?: boolean }) {
    const screen = electron.screen || electron.remote.screen;

    let maximize = false;
    targetBounds = Object.assign({}, targetBounds || await loadLastWindowsBounds(windowId));

    let originalBounds = window.getBounds();
    if(typeof options?.applySize === "boolean" && !options?.applySize) {
        targetBounds.width = originalBounds.width;
        targetBounds.height = originalBounds.height;
    }

    if(typeof options?.applyPosition === "boolean" && !options.applyPosition) {
        targetBounds.x = originalBounds.x;
        targetBounds.y = originalBounds.y;
    }

    if(targetBounds.width < 0 || targetBounds.height < 0) {
        /* Invalid bounds or may -1 and -1 for max window */
        const nearestScreen = screen.getDisplayNearestPoint({ x: targetBounds.x + 30, y: targetBounds.y + 30 });
        if(nearestScreen) {
            maximize = true;
            targetBounds = Object.assign({}, nearestScreen.workArea);
        } else {
            targetBounds.width = originalBounds.width;
            targetBounds.height = originalBounds.height;
        }
    }

    if(originalBounds.x === targetBounds.x &&
        originalBounds.y === targetBounds.y &&
        originalBounds.width === targetBounds.width &&
        originalBounds.height === targetBounds.height) {
        /* no changes */
        return;
    }

    /* Test if screen will be fully off display */
    /* FIXME: TODO! */

    window.setBounds(targetBounds, true);
    if(maximize) {
        window.maximize();
    }
}