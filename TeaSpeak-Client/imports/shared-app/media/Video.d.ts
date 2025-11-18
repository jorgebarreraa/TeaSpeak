import { ScreenCaptureDevice, VideoDevice, VideoDriver, VideoDriverEvents, VideoPermissionStatus, VideoSource, VideoSourceCapabilities, VideoSourceInitialSettings } from "tc-shared/video/VideoSource";
import { Registry } from "tc-shared/events";
declare global {
    interface MediaDevices {
        getDisplayMedia(options?: any): Promise<MediaStream>;
    }
}
export declare class WebVideoDriver implements VideoDriver {
    private readonly events;
    private currentPermissionStatus;
    constructor();
    private setPermissionStatus;
    private handleSystemPermissionState;
    initialize(): Promise<void>;
    getDevices(): Promise<VideoDevice[] | false>;
    requestPermissions(): Promise<VideoSource | boolean>;
    getEvents(): Registry<VideoDriverEvents>;
    getPermissionStatus(): VideoPermissionStatus;
    createVideoSource(id: string | undefined): Promise<VideoSource>;
    screenQueryAvailable(): boolean;
    queryScreenCaptureDevices(): Promise<ScreenCaptureDevice[]>;
    createScreenSource(_id: string | undefined, _allowFocusLoss: boolean): Promise<VideoSource>;
}
export declare class WebVideoSource implements VideoSource {
    private readonly deviceId;
    private readonly displayName;
    private readonly stream;
    private readonly initialSettings;
    private referenceCount;
    constructor(deviceId: string, displayName: string, stream: MediaStream);
    destroy(): void;
    getId(): string;
    getName(): string;
    getStream(): MediaStream;
    getInitialSettings(): VideoSourceInitialSettings;
    getCapabilities(): VideoSourceCapabilities;
    deref(): void;
    ref(): this;
}
