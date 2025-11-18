import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { ClientForumInfo, ClientInfoType, ClientStatusInfo, ClientVersionInfo } from "tc-shared/ui/frames/side/ClientInfoDefinitions";
import { ClientEntry } from "tc-shared/tree/Client";
import { Registry } from "tc-shared/events";
export declare type CachedClientInfoCategory = "name" | "description" | "online-state" | "country" | "volume" | "status" | "forum-account" | "group-channel" | "groups-server" | "version";
export declare type CachedClientInfo = {
    type: ClientInfoType;
    name: string;
    uniqueId: string;
    databaseId: number;
    clientId: number;
    description: string;
    joinTimestamp: number;
    leaveTimestamp: number;
    country: {
        name: string;
        flag: string;
    };
    volume: {
        volume: number;
        muted: boolean;
    };
    status: ClientStatusInfo;
    forumAccount: ClientForumInfo | undefined;
    channelGroup: number;
    channelGroupInheritedChannel: number;
    serverGroups: number[];
    version: ClientVersionInfo;
};
export interface ClientInfoManagerEvents {
    notify_client_changed: {
        newClient: ClientEntry | undefined;
    };
    notify_cache_changed: {
        category: CachedClientInfoCategory;
    };
}
export declare class SelectedClientInfo {
    readonly events: Registry<ClientInfoManagerEvents>;
    private readonly connection;
    private readonly listenerConnection;
    private listenerClient;
    private currentClient;
    private currentClientStatus;
    constructor(connection: ConnectionHandler);
    destroy(): void;
    getInfo(): CachedClientInfo;
    setClient(client: ClientEntry | undefined): void;
    getClient(): ClientEntry | undefined;
    private unregisterClientEvents;
    private registerClientEvents;
    private updateCachedClientStatus;
    private updateCachedCountry;
    private updateCachedVolume;
    private updateForumAccount;
    private updateChannelGroup;
    private initializeClientInfo;
}
