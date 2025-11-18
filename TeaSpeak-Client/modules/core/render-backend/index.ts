import "./menu";

import * as electron from "electron";
import ipcMain = electron.ipcMain;
import BrowserWindow = electron.BrowserWindow;

import {openChangeLog as open_changelog} from "../app-updater/changelog";
import {execute_connect_urls} from "../MultiInstanceHandler";
import {processArguments} from "../../shared/process-arguments";

import "./ExternalModal";
import {showUpdateWindow} from "../windows/client-updater/controller/ClientUpdate";
import {getUiFilePath} from "../ui-loader/Loader";

ipcMain.on('basic-action', (event, action, ...args: any[]) => {
    const window = BrowserWindow.fromWebContents(event.sender);

    if(action == "parse-connect-arguments") {
        execute_connect_urls(processArguments["_"] || []);
    } else if(action === "open-changelog") {
        open_changelog();
    } else if(action === "check-native-update") {
        showUpdateWindow().then(undefined);
    } else if(action === "open-dev-tools") {
        window.webContents.openDevTools();
    } else if(action === "reload-window") {
        window.reload();
    } else if(action === "quit") {
        window.close();
    }
});

ipcMain.handle("tc-get-ui-path", () => getUiFilePath());