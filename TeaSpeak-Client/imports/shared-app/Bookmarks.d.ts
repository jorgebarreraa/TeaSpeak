import { WritableKeys } from "tc-shared/proto";
import { Registry } from "tc-events";
declare type BookmarkBase = {
    readonly uniqueId: string;
    displayName: string;
    previousEntry: string | undefined;
    parentEntry: string | undefined;
};
export declare type BookmarkInfo = BookmarkBase & {
    readonly type: "entry";
    connectOnStartup: boolean;
    connectProfile: string;
    serverAddress: string;
    serverPasswordHash: string | undefined;
    defaultChannel: string | undefined;
    defaultChannelPasswordHash: string | undefined;
};
export declare type BookmarkDirectory = BookmarkBase & {
    readonly type: "directory";
};
export declare type BookmarkEntry = BookmarkInfo | BookmarkDirectory;
export interface BookmarkEvents {
    notify_bookmark_created: {
        bookmark: BookmarkEntry;
    };
    notify_bookmark_edited: {
        bookmark: BookmarkEntry;
        keys: (keyof BookmarkInfo | keyof BookmarkDirectory)[];
    };
    notify_bookmark_deleted: {
        bookmark: BookmarkEntry;
        children: BookmarkEntry[];
    };
    notify_bookmarks_imported: {
        bookmarks: BookmarkEntry[];
    };
}
export declare type OrderedBookmarkEntry = {
    entry: BookmarkEntry;
    depth: number;
    childCount: number;
};
export declare class BookmarkManager {
    readonly events: Registry<BookmarkEvents>;
    private readonly registeredBookmarks;
    private defaultBookmarkCreated;
    constructor();
    loadBookmarks(): Promise<void>;
    private importOldBookmarks;
    saveBookmarks(): Promise<void>;
    getRegisteredBookmarks(): BookmarkEntry[];
    getOrderedRegisteredBookmarks(): OrderedBookmarkEntry[];
    findBookmark(uniqueId: string): BookmarkEntry | undefined;
    createBookmark(properties: Pick<BookmarkInfo, WritableKeys<BookmarkInfo>>): BookmarkInfo;
    editBookmark(uniqueId: string, newValues: Partial<Pick<BookmarkInfo, WritableKeys<BookmarkInfo>>>): void;
    createDirectory(properties: Pick<BookmarkInfo, WritableKeys<BookmarkDirectory>>): BookmarkDirectory;
    editDirectory(uniqueId: string, newValues: Partial<Pick<BookmarkDirectory, WritableKeys<BookmarkDirectory>>>): void;
    deleteEntry(uniqueId: string): void;
    executeConnect(uniqueId: string, newTab: boolean): void;
    executeAutoConnect(): void;
    exportBookmarks(): string;
    importBookmarks(filePayload: string): number;
    private doEditBookmark;
    private validateHangInPoint;
}
export declare let bookmarks: BookmarkManager;
export {};
