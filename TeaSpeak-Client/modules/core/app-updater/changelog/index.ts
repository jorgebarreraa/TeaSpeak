import {BrowserWindow, dialog} from "electron";
import * as electron from "electron";
import * as path from "path";
import * as url from "url";

let changeLogWindow: BrowserWindow;
export function openChangeLog() {
    if(changeLogWindow) {
        changeLogWindow.focus();
        return;
    }

    changeLogWindow = new BrowserWindow({
        show: false
    });

    changeLogWindow.setMenu(null);

    let file;
    {
        const appPath = electron.app.getAppPath();
        if(appPath.endsWith(".asar")) {
            file = path.join(path.dirname(appPath), "..", "ChangeLog.txt");
        } else {
            file = path.join(appPath, "github", "ChangeLog.txt"); /* We've the source ;) */
        }
    }

    changeLogWindow.loadURL(url.pathToFileURL(file).toString()).catch(error => {
        console.error("Failed to open changelog: %o", error);
        dialog.showErrorBox("Failed to open the ChangeLog", "Failed to open the changelog file.\nLookup the console for more details.");
        closeChangeLog();
    });

    changeLogWindow.setTitle("TeaClient ChangeLog");
    changeLogWindow.on('ready-to-show', () => {
        changeLogWindow.show();
    });

    changeLogWindow.on('close', () => {
        changeLogWindow = undefined;
    });
}

export function closeChangeLog() {
    if(changeLogWindow) {
        changeLogWindow.close();
        changeLogWindow = undefined;
    }
}