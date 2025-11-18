import {
    WindowAction,
    WindowCreateResult,
    WindowManager,
    WindowManagerEvents,
    WindowSpawnOptions
} from "tc-shared/ui/windows/WindowManager";
import {Registry} from "tc-events";
import {assertMainApplication} from "tc-shared/ui/utils";
import {ObjectProxyClient} from "../shared/proxy/Client";
import {IpcWindowInstance, kIPCChannelWindowManager} from "../shared/ipc/IpcWindowInstance";
import {ProxiedClass} from "../shared/proxy/Definitions";
import {guid} from "tc-shared/crypto/uid";
import {getIpcInstance} from "tc-shared/ipc/BrowserIPC";

assertMainApplication();

const modalClient = new ObjectProxyClient<IpcWindowInstance>(kIPCChannelWindowManager);
modalClient.initialize();

export class NativeWindowManager implements WindowManager {
    private readonly events: Registry<WindowManagerEvents>;
    private readonly windowInstances: { [key: string]: ProxiedClass<IpcWindowInstance> & IpcWindowInstance };

    constructor() {
        this.windowInstances = {};
        this.events = new Registry<WindowManagerEvents>();

        window.onunload = () => {
            Object.values(this.windowInstances).forEach(window => window.destroy());
        };
    }

    getEvents(): Registry<WindowManagerEvents> {
        return this.events;
    }

    async createWindow(options: WindowSpawnOptions): Promise<WindowCreateResult> {
        const windowHandle = await modalClient.createNewInstance();

        const parameters = {
            "loader-target": "manifest",
            "loader-chunk": "modal-external",
            "ipc-address": getIpcInstance().getApplicationChannelId(),
            "ipc-core-peer": getIpcInstance().getLocalPeerId(),
            "loader-abort": 0,
            "animation-short": 1
        };
        Object.assign(parameters, options.appParameters);

        const baseUrl = location.origin + location.pathname + "?";
        const url = baseUrl + Object.keys(parameters).map(e => e + "=" + encodeURIComponent(parameters[e])).join("&");
        const result = await windowHandle.initialize("modal-external", options.uniqueId, url);
        if(!result) {
            windowHandle.destroy();
            return { status: "error-unknown", message: tr("failed to spawn window") };
        }

        const windowId = guid();
        windowHandle.events.onClose = () => this.destroyWindow(windowId);
        this.windowInstances[windowId] = windowHandle;
        return { status: "success", windowId: windowId };
    }

    destroyWindow(windowId: string): any {
        if(!this.windowInstances[windowId]) {
            return;
        }

        this.windowInstances[windowId].destroy();
        delete this.windowInstances[windowId];
        this.events.fire("notify_window_destroyed", { windowId: windowId });
    }

    getActiveWindowId(): string | undefined {
        /* TODO! */
        return undefined;
    }

    async executeAction(windowId: string, action: WindowAction): Promise<void> {
        if(!this.windowInstances[windowId]) {
            return;
        }

        const window = this.windowInstances[windowId];
        switch (action) {
            case "focus":
                await window.focus();
                break;

            case "maximize":
                await window.maximize();
                break;

            case "minimize":
                await window.minimize();
                break;
        }
    }

    isActionSupported(windowId: string, action: WindowAction): boolean {
        switch (action) {
            case "minimize":
            case "focus":
            case "maximize":
                return true;

            default:
                return false;
        }
    }
}