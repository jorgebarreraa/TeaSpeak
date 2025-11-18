import { InputStartError } from "tc-shared/voice/RecorderBase";
export declare type MediaStreamType = "audio" | "video";
export declare enum MediaPermissionStatus {
    Unknown = 0,
    Granted = 1,
    Denied = 2
}
export declare function requestMediaStreamWithConstraints(constraints: MediaTrackConstraints, type: MediaStreamType): Promise<InputStartError | MediaStream>;
export declare function requestMediaStream(deviceId: string | undefined, groupId: string | undefined, type: MediaStreamType): Promise<MediaStream | InputStartError>;
export declare function queryMediaPermissions(type: MediaStreamType, changeListener?: (value: PermissionState) => void): Promise<PermissionState>;
export declare function stopMediaStream(stream: MediaStream): void;
