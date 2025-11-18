import { FileManager } from "../file/FileManager";
import { FileDownloadTransfer } from "../file/Transfer";
import { AbstractAvatarManager, ClientAvatar } from "../file/Avatars";
export declare class AvatarManager extends AbstractAvatarManager {
    readonly handle: FileManager;
    private cachedAvatars;
    constructor(handle: FileManager);
    destroy(): void;
    create_avatar_download(client_avatar_id: string): FileDownloadTransfer;
    private executeAvatarLoad0;
    private executeAvatarLoad;
    updateCache(clientAvatarId: string, clientAvatarHash: string): Promise<void>;
    resolveAvatar(clientAvatarId: string, avatarHash?: string, cacheOnly?: boolean): ClientAvatar;
    resolveClientAvatar(client: {
        id?: number;
        database_id?: number;
        clientUniqueId: string;
    }): ClientAvatar;
    private static generate_default_image;
    generate_chat_tag(client: {
        id?: number;
        database_id?: number;
    }, client_unique_id: string, callback_loaded?: (successfully: boolean, error?: any) => any): JQuery;
    flush_cache(): void;
}
