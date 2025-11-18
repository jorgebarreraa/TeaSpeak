import * as electron from "electron";
import ipcMain = electron.ipcMain;
import BrowserWindow = electron.BrowserWindow;
import {NativeMenuBarEntry} from "../../shared/MenuBarDefinitions";
import {Menu, MenuItemConstructorOptions} from "electron";

ipcMain.on("menu-bar", (event, menuBar: NativeMenuBarEntry[]) => {
    const window = BrowserWindow.fromWebContents(event.sender);

    try {
        const processEntry = (entry: NativeMenuBarEntry): MenuItemConstructorOptions => {
            return {
                type: entry.type === "separator" ? "separator" : entry.children?.length ? "submenu" : "normal",
                label: entry.label,
                icon: entry.icon ? electron.nativeImage.createFromDataURL(entry.icon).resize({ height: 16, width: 16 }) : undefined,
                enabled: !entry.disabled,
                click: entry.uniqueId && (() => event.sender.send("menu-bar", "item-click", entry.uniqueId)),
                submenu: entry.children?.map(processEntry)
            }
        };

        window.setMenu(Menu.buildFromTemplate(menuBar.map(processEntry)));
    } catch (error) {
        console.error("failed to set menu bar for %s: %o", window.getTitle(), error);
    }
});