import {app, BrowserWindow} from "electron";
import * as crash_handler from "../crash_handler";

let appReferences = 0;
let windowOpen = false;

/**
 * Normally the app closes when all windows have been closed.
 * If you're holding an app reference, it will not terminate when all windows have been closed.
 */
export function referenceApp() {
    appReferences++;
}

export function dereferenceApp() {
    appReferences--;
    testAppState();
}

function testAppState() {
    if(appReferences > 0) { return; }
    if(windowOpen) { return; }

    console.log("All windows have been closed, closing app.");
    app.quit();
}

function initializeAppListeners() {
    app.on('quit', () => {
        console.debug("Shutting down app.");
        crash_handler.finalize_handler();
        console.log("App has been finalized.");
    });


    app.on('window-all-closed', () => {
        windowOpen = false;
        console.log("All windows have been closed. Manual app reference count: %d", appReferences);
        testAppState();
    });

    app.on("browser-window-created", () => {
        windowOpen = true;
    })

    app.on('activate', () => {
        // On macOS it's common to re-create a window in the app when the
        // dock icon is clicked and there are no other windows open.
    });
}
initializeAppListeners();