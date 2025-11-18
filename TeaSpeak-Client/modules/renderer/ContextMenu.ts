import {ContextMenuEntry, ContextMenuFactory} from "tc-shared/ui/ContextMenu";
import * as electron from "electron";
import {MenuItemConstructorOptions} from "electron";
import {clientIconClassToImage, remoteIconDatafier, RemoteIconWrapper} from "./IconHelper";
import {getIconManager, RemoteIconInfo} from "tc-shared/file/Icons";
const {Menu} = electron.remote;

let currentMenu: ContextMenuInstance;
class ContextMenuInstance {
    private readonly closeCallback: () => void | undefined;
    private readonly menuOptions: MenuItemConstructorOptions[];
    private currentMenu: electron.Menu;

    private wrappedIcons: RemoteIconWrapper[] = [];
    private wrappedIconListeners: (() => void)[] = [];

    constructor(entries: ContextMenuEntry[], closeCallback: () => void | undefined) {
        this.closeCallback = closeCallback;
        this.menuOptions = entries.map(e => this.wrapEntry(e)).filter(e => !!e);
    }

    destroy() {
        this.currentMenu?.closePopup();
        this.currentMenu = undefined;

        this.wrappedIconListeners.forEach(callback => callback());
        this.wrappedIcons.forEach(icon => remoteIconDatafier.unrefIcon(icon));

        this.wrappedIcons = [];
        this.wrappedIconListeners = [];
    }

    spawn(pageX: number, pageY: number) {
        this.currentMenu = Menu.buildFromTemplate(this.menuOptions);
        this.currentMenu.popup({
            callback: () => {
                if(this.closeCallback) {
                    this.closeCallback();
                }
                currentMenu = undefined;
            },
            x: pageX,
            y: pageY,
            window: electron.remote.BrowserWindow.getFocusedWindow()
        });
    }

    private wrapEntry(entry: ContextMenuEntry) : MenuItemConstructorOptions {
        if(typeof entry.visible === "boolean" && !entry.visible) { return undefined; }

        let options: MenuItemConstructorOptions;
        let icon: string | RemoteIconInfo | undefined;

        switch (entry.type) {
            case "normal":
                icon = entry.icon;
                options = {
                    type: entry.subMenu?.length ? "submenu" : "normal",
                    label: typeof entry.label === "string" ? entry.label : entry.label.text,
                    enabled: entry.enabled,
                    click: entry.click,
                    id: entry.uniqueId,
                    submenu: entry.subMenu?.length ? entry.subMenu.map(e => this.wrapEntry(e)).filter(e => !!e) : undefined
                };
                break;

            case "checkbox":
                icon = entry.icon;
                options = {
                    type: "checkbox",
                    label: typeof entry.label === "string" ? entry.label : entry.label.text,
                    enabled: entry.enabled,
                    click: entry.click,
                    id: entry.uniqueId,
                    checked: entry.checked
                };
                break;

            case "separator":
                return {
                    type: "separator"
                };

            default:
                return undefined;
        }

        if(typeof icon === "object") {
            const remoteIcon = getIconManager().resolveIcon(icon.iconId, icon.serverUniqueId, icon.handlerId);
            const wrapped = remoteIconDatafier.resolveIcon(remoteIcon);
            remoteIconDatafier.unrefIcon(wrapped);

            /*
            // Sadly we can't update the icon on the fly, so we've to live with whatever we have
            this.wrappedIcons.push(wrapped);
            this.wrappedIconListeners.push(wrapped.onDataUrlChange(dataUrl => {
                options.icon = electron.nativeImage.createFromDataURL(dataUrl);
            }));
            */

            if(wrapped.getDataUrl()) {
                options.icon = electron.remote.nativeImage.createFromDataURL(wrapped.getDataUrl());
            }
        } else if(typeof icon === "string") {
            options.icon = clientIconClassToImage(icon);
        }

        return options;
    }
}

export class ClientContextMenuFactory implements ContextMenuFactory {
    closeContextMenu() {
        currentMenu?.destroy();
        currentMenu = undefined;
    }

    spawnContextMenu(position: { pageX: number; pageY: number }, entries: ContextMenuEntry[], callbackClose?: () => void) {
        this.closeContextMenu();
        currentMenu = new ContextMenuInstance(entries, callbackClose);
        currentMenu.spawn(position.pageX, position.pageY);
    }
};