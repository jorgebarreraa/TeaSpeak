export declare type LocalAvatarInfo = {
    fileName: string;
    fileSize: number;
    fileHashMD5: string;
    fileUploaded: number;
    fileModified: number;
    contentType: string;
    resourceUrl: string | undefined;
};
export declare type LocalAvatarUpdateResult = {
    status: "success";
} | {
    status: "error";
    reason: string;
} | {
    status: "cache-unavailable";
};
export declare type LocalAvatarLoadResult<T> = {
    status: "success";
    result: T;
} | {
    status: "error";
    reason: string;
} | {
    status: "cache-unavailable" | "empty-result";
};
export declare type OwnAvatarMode = "uploading" | "server";
export declare class OwnAvatarStorage {
    private openedCache;
    private static generateRequestUrl;
    initialize(): Promise<void>;
    private loadAvatarRequest;
    loadAvatarImage(serverUniqueId: string, mode: OwnAvatarMode): Promise<LocalAvatarLoadResult<ArrayBuffer>>;
    loadAvatar(serverUniqueId: string, mode: OwnAvatarMode, createResourceUrl: boolean): Promise<LocalAvatarLoadResult<LocalAvatarInfo>>;
    updateAvatar(serverUniqueId: string, mode: OwnAvatarMode, target: File): Promise<LocalAvatarUpdateResult>;
    removeAvatar(serverUniqueId: string, mode: OwnAvatarMode): Promise<void>;
    /**
     * Move the avatar file which is currently in "uploading" state to server
     * @param serverUniqueId
     */
    avatarUploadSucceeded(serverUniqueId: string): Promise<void>;
}
export declare function getOwnAvatarStorage(): OwnAvatarStorage;
