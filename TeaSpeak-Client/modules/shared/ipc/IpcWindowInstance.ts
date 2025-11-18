export const kIPCChannelWindowManager = "window-manager";

export interface IpcWindowInstance {
    readonly events: {
        onClose: () => void
    }

    initialize(loaderTarget: string, windowId: string, url: string) : Promise<boolean>;
    minimize() : Promise<void>;
    maximize() : Promise<void>;
    focus() : Promise<void>;
}