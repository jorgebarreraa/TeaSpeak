import * as electron from "electron";
import {MenuBarDriver, MenuBarEntry} from "tc-shared/ui/frames/menu-bar";
import {IpcRendererEvent} from "electron";
import {getIconManager} from "tc-shared/file/Icons";
import {clientIconClassToImage, remoteIconDatafier, RemoteIconWrapper} from "./IconHelper";
import {NativeMenuBarEntry} from "../shared/MenuBarDefinitions";

let uniqueEntryIdIndex = 0;
export class NativeMenuBarDriver implements MenuBarDriver {
    private readonly ipcChannelListener;

    private menuEntries: NativeMenuBarEntry[] = [];
    private remoteIconReferences: RemoteIconWrapper[] = [];
    private remoteIconListeners: (() => void)[] = [];
    private callbacks: {[key: string]: () => void} = {};

    constructor() {
        this.ipcChannelListener = this.handleMenuBarEvent.bind(this);

        electron.ipcRenderer.on("menu-bar", this.ipcChannelListener);
    }

    destroy() {
        electron.ipcRenderer.off("menu-bar", this.ipcChannelListener);
        this.internalClearEntries();
    }

    private internalClearEntries() {
        this.callbacks = {};
        this.menuEntries = [];

        this.remoteIconListeners.forEach(callback => callback());
        this.remoteIconListeners = [];

        this.remoteIconReferences.forEach(icon => remoteIconDatafier.unrefIcon(icon));
        this.remoteIconReferences = [];
    }

    clearEntries() {
        this.internalClearEntries()
        electron.ipcRenderer.send("menu-bar", []);
    }

    setEntries(entries: MenuBarEntry[]) {
        this.internalClearEntries();
        this.menuEntries = entries.map(e => this.wrapEntry(e)).filter(e => !!e);
        electron.ipcRenderer.send("menu-bar", this.menuEntries);
    }

    private wrapEntry(entry: MenuBarEntry) : NativeMenuBarEntry {
        if(entry.type === "separator") {
            return { type: "separator", uniqueId: entry.uniqueId || "item-" + (++uniqueEntryIdIndex) };
        } else if(entry.type === "normal") {
            if(typeof entry.visible === "boolean" && !entry.visible) {
                return null;
            }

            let result = {
                type: "normal",
                uniqueId: entry.uniqueId || "item-" + (++uniqueEntryIdIndex),
                label: entry.label,
                disabled: entry.disabled,
                children: entry.children?.map(e => this.wrapEntry(e)).filter(e => !!e)
            } as NativeMenuBarEntry;

            if(entry.click) {
                this.callbacks[result.uniqueId] = entry.click;
            }

            if(typeof entry.icon === "object") {
                /* we've a remote icon */
                const remoteIcon = getIconManager().resolveIcon(entry.icon.iconId, entry.icon.serverUniqueId, entry.icon.handlerId);
                const wrapped = remoteIconDatafier.resolveIcon(remoteIcon);
                this.remoteIconReferences.push(wrapped);

                wrapped.onDataUrlChange(url => {
                    result.icon = url;
                    electron.ipcRenderer.send("menu-bar", this.menuEntries);
                });
                result.icon = wrapped.getDataUrl();
            } else if(typeof entry.icon === "string") {
                result.icon = clientIconClassToImage(entry.icon).toDataURL();
            }

            return result;
        } else {
            return undefined;
        }
    }

    private handleMenuBarEvent(_event: IpcRendererEvent, eventType: string, ...args) {
        if(eventType === "item-click") {
            const callback = this.callbacks[args[0]];
            if(typeof callback === "function") {
                callback();
            }
        }
    }
}