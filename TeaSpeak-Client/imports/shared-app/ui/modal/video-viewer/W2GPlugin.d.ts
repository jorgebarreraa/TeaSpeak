import { PlayerStatus } from "../video-viewer/Definitions";
import { Registry } from "tc-events";
import { PluginCmdHandler, PluginCommandInvoker } from "tc-shared/connection/PluginCmdHandler";
export interface W2GEvents {
    notify_watcher_add: {
        watcher: W2GWatcher;
    };
    notify_watcher_remove: {
        watcher: W2GWatcher;
    };
    notify_following_changed: {
        oldWatcher: W2GWatcher | undefined;
        newWatcher: W2GWatcher | undefined;
    };
    notify_following_watcher_status: {
        newStatus: PlayerStatus;
    };
    notify_following_url: {
        newUrl: string;
    };
}
export interface W2GWatcherEvents {
    notify_follower_added: {
        follower: W2GWatcherFollower;
    };
    notify_follower_removed: {
        follower: W2GWatcherFollower;
    };
    notify_follower_status_changed: {
        follower: W2GWatcherFollower;
        newStatus: PlayerStatus;
    };
    notify_follower_nickname_changed: {
        follower: W2GWatcherFollower;
        newName: string;
    };
    notify_watcher_status_changed: {
        newStatus: PlayerStatus;
    };
    notify_watcher_nickname_changed: {
        newName: string;
    };
    notify_watcher_url_changed: {
        oldVideo: string;
        newVideo: string;
    };
    notify_destroyed: {};
}
export interface W2GWatcherFollower {
    clientId: number;
    clientUniqueId: string;
    clientNickname: string;
    status: PlayerStatus;
}
export declare abstract class W2GWatcher {
    readonly events: Registry<W2GWatcherEvents>;
    readonly clientId: number;
    readonly clientUniqueId: string;
    protected constructor(clientId: any, clientUniqueId: any);
    abstract getWatcherName(): string;
    abstract getWatcherStatus(): PlayerStatus;
    abstract getCurrentVideo(): string;
    abstract getFollowers(): W2GWatcherFollower[];
}
export declare class W2GPluginCmdHandler extends PluginCmdHandler {
    static readonly kPluginChannel = "teaspeak-w2g";
    static readonly kStatusUpdateInterval = 5000;
    static readonly kStatusUpdateTimeout = 10000;
    readonly events: Registry<W2GEvents>;
    private readonly callbackWatcherEvents;
    private currentWatchers;
    private localPlayerStatus;
    private localVideoUrl;
    private localFollowing;
    private localStatusUpdateTimer;
    constructor();
    handleHandlerRegistered(): void;
    handleHandlerUnregistered(): void;
    handlePluginCommand(data: string, invoker: PluginCommandInvoker): void;
    private sendCommand;
    getCurrentWatchers(): W2GWatcher[];
    private findWatcher;
    private destroyWatcher;
    private removeClientFromWatchers;
    private removeClientFromFollowers;
    private handlePlayerClosed;
    private handleStatusUpdate;
    private watcherStatusTimeout;
    private notifyLocalStatus;
    setLocalPlayerClosed(): void;
    setLocalWatcherStatus(videoUrl: string, status: PlayerStatus): void;
    setLocalFollowing(target: W2GWatcher | undefined, status?: PlayerStatus): void;
    getLocalFollowingWatcher(): W2GWatcher | undefined;
    private handleLocalWatcherEvent;
}
