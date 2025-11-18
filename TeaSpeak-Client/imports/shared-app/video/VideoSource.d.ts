import { Registry } from "tc-shared/events";
export interface VideoSourceCapabilities {
    minWidth: number;
    maxWidth: number;
    minHeight: number;
    maxHeight: number;
    minFrameRate: number;
    maxFrameRate: number;
}
export interface VideoSourceInitialSettings {
    width: number | 0;
    height: number | 0;
    frameRate: number;
}
export interface VideoSource {
    getId(): string;
    getName(): string;
    getStream(): MediaStream;
    getCapabilities(): VideoSourceCapabilities;
    getInitialSettings(): VideoSourceInitialSettings;
    /** Add a new reference to this stream */
    ref(): this;
    /** Decrease the reference count. If it's zero, it will be automatically destroyed. */
    deref(): any;
}
export declare enum VideoPermissionStatus {
    Granted = 0,
    UserDenied = 1,
    SystemDenied = 2
}
export interface VideoDriverEvents {
    notify_permissions_changed: {
        oldStatus: VideoPermissionStatus;
        newStatus: VideoPermissionStatus;
    };
    notify_device_list_changed: {
        devices: string[];
    };
}
export declare type VideoDevice = {
    id: string;
    name: string;
};
export declare type ScreenCaptureDevice = {
    id: string;
    name: string;
    type: "full-screen" | "window";
    appIcon?: string;
    appPreview?: string;
};
export interface VideoDriver {
    getEvents(): Registry<VideoDriverEvents>;
    getPermissionStatus(): VideoPermissionStatus;
    /**
     * Request permissions to access the video camara and device list.
     * When requesting permissions, we're actually requesting a media stream.
     * If the request succeeds, we're returning that media stream.
     */
    requestPermissions(): Promise<VideoSource | boolean>;
    getDevices(): Promise<VideoDevice[] | false>;
    /**
     * @throws a string if an error occurs
     * @returns A VideoSource on success with an initial ref count of one
     * Will throw a string on error
     */
    createVideoSource(id: string | undefined): Promise<VideoSource>;
    screenQueryAvailable(): boolean;
    queryScreenCaptureDevices(): Promise<ScreenCaptureDevice[]>;
    /**
     * Create a source from the screen
     */
    createScreenSource(id: string | undefined, allowFocusLoss: boolean): Promise<VideoSource>;
}
export declare function getVideoDriver(): VideoDriver;
export declare function setVideoDriver(driver: VideoDriver): void;
