import * as electron from "electron";
import * as path from "path";
import {loadWindowBounds, startTrackWindowBounds} from "../../shared/window";

let global_window: electron.BrowserWindow;
let global_window_promise: Promise<void>;

export async function closeURLPreview() {
    while(global_window_promise) {
        try {
            await global_window_promise;
            break;
        } catch(error) {} /* error will be already logged */
    }

    if(global_window) {
        global_window.close();
        global_window = undefined;
        global_window_promise = undefined;
    }
}

export async function openURLPreview(url: string) {
    while(global_window_promise) {
        try {
            await global_window_promise;
            break;
        } catch(error) {} /* error will be already logged */
    }

    if(!global_window) {
        global_window_promise = (async () => {
            global_window = new electron.BrowserWindow({
                webPreferences: {
                    nodeIntegration: true,
                    webviewTag: true
                },
                center: true,
                show: false,

                minHeight: 400,
                minWidth: 400
            });
            global_window.setMenuBarVisibility(false);
            global_window.setMenu(null);
            global_window.loadURL(require("url").pathToFileURL(path.join(__dirname, "html", "index.html")).toString()).then(() => {
                //global_window.webContents.openDevTools();
            });
            global_window.on('close', event => {
                global_window = undefined;
            });

            try {
                await loadWindowBounds('url-preview', global_window);
                startTrackWindowBounds('url-preview', global_window);

                await new Promise((resolve, reject) => {
                    const timeout = setTimeout(() => reject("timeout"), 5000);
                    global_window.on('ready-to-show', () => {
                        clearTimeout(timeout);
                        resolve();
                    });
                });
            } catch(error) {
                console.warn("Failed to initialize preview window. Dont show preview! Error: %o", error);
                throw "failed to initialize";
            }

            global_window.show();
        })();
        try {
            await global_window_promise;
        } catch(error) {
            console.log("Failed to create preview window! Error: %o", error);
            try {
                global_window.close();
            } finally {
                global_window = undefined;
            }
            global_window_promise = undefined;
            return;
        }
    }

    console.log("Opening URL '%s' as preview.", url);
    global_window.webContents.send('preview', url);
    if(!global_window.isFocused())
        global_window.focus();
}

electron.ipcMain.on('preview-action', (event, args) => {
    const sender: electron.WebContents = event.sender;
    if(!args || !args.action) {
        console.warn("Received preview action without a valid action type!");
        return;
    }

    if(args.action === "open-url") {
        console.log("Opening " +args.url);
        electron.shell.openExternal(args.url, {
            activate: true
        });

        const browser = electron.BrowserWindow.fromWebContents(sender);
        if(!browser)
            console.warn("Failed to find browser handle");
        else
            browser.close();
    }
});