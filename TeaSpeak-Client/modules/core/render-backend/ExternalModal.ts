import {ObjectProxyServer} from "../../shared/proxy/Server";
import {IpcWindowInstance, kIPCChannelWindowManager} from "../../shared/ipc/IpcWindowInstance";
import {ProxiedClass} from "../../shared/proxy/Definitions";
import {BrowserWindow, dialog} from "electron";
import {loadWindowBounds, startTrackWindowBounds} from "../../shared/window";
import {Arguments, processArguments} from "../../shared/process-arguments";
import {openURLPreview} from "../url-preview";
import * as path from "path";

class ProxyImplementation extends ProxiedClass<IpcWindowInstance> implements IpcWindowInstance {
    private windowInstance: BrowserWindow;

    async focus(): Promise<void> {
        this.windowInstance?.focusOnWebView();
    }

    async minimize(): Promise<void> {
        this.windowInstance?.minimize();
    }

    async maximize(): Promise<void> {
        this.windowInstance?.maximize();
    }

    async initialize(loaderTarget: string, windowId: string, url: string): Promise<boolean> {
        if(this.windowInstance) {
            return false;
        }

        this.windowInstance = new BrowserWindow({
            /* parent: remote.getCurrentWindow(), */ /* do not link them together */
            autoHideMenuBar: true,

            webPreferences: {
                nodeIntegration: true,
                preload: path.join(__dirname, "..", "..", "renderer-manifest", "preload.js")
            },
            icon: path.join(__dirname, "..", "..", "resources", "logo.ico"),
            minWidth: 600,
            minHeight: 300,

            frame: false,
            transparent: true,

            show: true
        });

        loadWindowBounds("window-" + windowId, this.windowInstance).then(() => {
            startTrackWindowBounds("window-" + windowId, this.windowInstance);
        });

        this.windowInstance.webContents.on('new-window', (event, url_str, frameName, disposition, options, additionalFeatures) => {
            console.error("Open: %O", frameName);
            if(frameName.startsWith("__modal_external__")) {
                return;
            }

            event.preventDefault();
            try {
                let url: URL;
                try {
                    url = new URL(url_str);
                } catch(error) {
                    throw "failed to parse URL";
                }

                {
                    let protocol = url.protocol.endsWith(":") ? url.protocol.substring(0, url.protocol.length - 1) : url.protocol;
                    if(protocol !== "https" && protocol !== "http") {
                        throw "invalid protocol (" + protocol + "). HTTP(S) are only supported!";
                    }
                }

                openURLPreview(url.toString());
            } catch(error) {
                console.error("Failed to open preview window for URL %s: %o", url_str, error);
                dialog.showErrorBox("Failed to open preview", "Failed to open preview URL: " + url_str + "\nError: " + error);
            }
        });

        if(processArguments.has_flag(Arguments.DEV_TOOLS)) {
            this.windowInstance.webContents.openDevTools();
        }

        try {
            await this.windowInstance.loadURL(url);
        } catch (error) {
            console.error("Failed to load external modal main page: %o", error);
            this.windowInstance.close();
            this.windowInstance = undefined;
            return false;
        }

        this.windowInstance.on("closed", () => {
            this.windowInstance = undefined;
            this.events.onClose();
        });

        return true;
    }

    destroy() {
        if(!this.windowInstance) {
            return;
        }

        this.windowInstance.close();
        this.windowInstance = undefined;
    }
}

const server = new ObjectProxyServer<IpcWindowInstance>(kIPCChannelWindowManager, ProxyImplementation);
server.initialize();