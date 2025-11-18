import { Registry } from "tc-events";
export declare type WindowCreateResult = {
    status: "success";
    windowId: string;
} | {
    status: "error-unknown";
    message: string;
} | {
    status: "error-user-rejected";
};
export interface WindowManagerEvents {
    notify_window_created: {
        windowId: string;
    };
    notify_window_focused: {
        windowId: string;
    };
    notify_window_destroyed: {
        windowId: string;
    };
}
export declare type WindowAction = "focus" | "maximize" | "minimize";
export interface WindowSpawnOptions {
    uniqueId: string;
    loaderTarget: string;
    windowName?: string;
    appParameters?: {
        [key: string]: string;
    };
    defaultSize?: {
        width: number;
        height: number;
    };
}
export interface WindowManager {
    getEvents(): Registry<WindowManagerEvents>;
    createWindow(options: WindowSpawnOptions): Promise<WindowCreateResult>;
    destroyWindow(windowId: string): any;
    getActiveWindowId(): string | undefined;
    isActionSupported(windowId: string, action: WindowAction): boolean;
    executeAction(windowId: string, action: WindowAction): Promise<void>;
}
export declare function getWindowManager(): WindowManager;
export declare function setWindowManager(newWindowManager: WindowManager): void;
