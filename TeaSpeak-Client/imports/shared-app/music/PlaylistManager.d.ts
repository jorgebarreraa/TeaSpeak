import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { Registry } from "tc-shared/events";
export declare type PlaylistEntry = {
    type: "song";
    id: number;
    previousId: number;
    url: string;
    urlLoader: string;
    invokerDatabaseId: number;
    metadata: PlaylistSongMetadata;
};
export declare type PlaylistSongMetadata = {
    status: "loading";
} | {
    status: "unparsed";
    metadata: string;
} | {
    status: "loaded";
    metadata: string;
    title: string;
    description: string;
    thumbnailUrl?: string;
    length: number;
};
export interface SubscribedPlaylistEvents {
    notify_status_changed: {};
    notify_entry_added: {
        entry: PlaylistEntry;
    };
    notify_entry_deleted: {
        entry: PlaylistEntry;
    };
    notify_entry_reordered: {
        entry: PlaylistEntry;
        oldPreviousId: number;
    };
    notify_entry_updated: {
        entry: PlaylistEntry;
    };
}
export declare type SubscribedPlaylistStatus = {
    status: "loaded";
    songs: PlaylistEntry[];
} | {
    status: "loading";
} | {
    status: "error";
    error: string;
} | {
    status: "no-permissions";
    failedPermission: string;
} | {
    status: "unloaded";
};
export declare abstract class SubscribedPlaylist {
    readonly events: Registry<SubscribedPlaylistEvents>;
    readonly playlistId: number;
    readonly serverUniqueId: string;
    protected status: SubscribedPlaylistStatus;
    protected refCount: number;
    protected constructor(serverUniqueId: string, playlistId: number);
    ref(): SubscribedPlaylist;
    unref(): void;
    destroy(): void;
    /**
     * Query the playlist songs from the remote server.
     * The playlist status will change on a successfully or failed query.
     *
     * @param forceQuery Forcibly query even we're subscribed and already aware of all songs.
     */
    abstract querySongs(forceQuery: boolean): Promise<void>;
    abstract addSong(url: string, urlLoader: "any" | "youtube" | "ffmpeg" | "channel", targetSongId: number | 0, mode?: "before" | "after" | "last"): Promise<void>;
    abstract deleteEntry(entryId: number): Promise<void>;
    abstract reorderEntry(entryId: number, targetEntryId: number, mode: "before" | "after"): Promise<void>;
    getStatus(): Readonly<SubscribedPlaylistStatus>;
    protected setStatus(status: SubscribedPlaylistStatus): void;
}
export declare class PlaylistManager {
    readonly connection: ConnectionHandler;
    private listenerConnection;
    private playlistEntryListCache;
    private subscribedPlaylist;
    constructor(connection: ConnectionHandler);
    destroy(): void;
    queryPlaylistEntries(playlistId: number): Promise<PlaylistEntry[]>;
    reorderSong(playlistId: number, songId: number, previousSongId: number): Promise<void>;
    addSong(playlistId: number, url: string, urlLoader: "any" | "youtube" | "ffmpeg" | "channel", previousSongId: number | 0): Promise<void>;
    removeSong(playlistId: number, entryId: number): Promise<void>;
    /**
     * @param playlistId
     * @return Returns a subscribed playlist.
     * Attention: You have to manually destroy the object!
     */
    createSubscribedPlaylist(playlistId: number): SubscribedPlaylist;
}
