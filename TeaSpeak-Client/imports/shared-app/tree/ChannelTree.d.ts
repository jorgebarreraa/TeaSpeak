import { Group } from "tc-shared/permission/GroupManager";
import { ServerAddress, ServerEntry } from "./Server";
import { ChannelEntry, ChannelProperties } from "./Channel";
import { ClientEntry } from "./Client";
import { ChannelTreeEntry } from "./ChannelTreeEntry";
import { ConnectionHandler, ViewReasonId } from "tc-shared/ConnectionHandler";
import { Registry } from "tc-shared/events";
import { ChannelTreePopoutController } from "tc-shared/ui/tree/popout/Controller";
import "./EntryTagsHandler";
import { ChannelTreeUIEvents } from "tc-shared/ui/tree/Definitions";
export interface ChannelTreeEvents {
    notify_tree_reset: {};
    notify_query_view_state_changed: {
        queries_shown: boolean;
    };
    notify_popout_state_changed: {
        popoutShown: boolean;
    };
    notify_entry_move_begin: {};
    notify_entry_move_end: {};
    notify_channel_created: {
        channel: ChannelEntry;
    };
    notify_channel_moved: {
        channel: ChannelEntry;
        previousParent: ChannelEntry | undefined;
        previousOrder: ChannelEntry | undefined;
    };
    notify_channel_deleted: {
        channel: ChannelEntry;
    };
    notify_channel_client_order_changed: {
        channel: ChannelEntry;
    };
    notify_channel_updated: {
        channel: ChannelEntry;
        channelProperties: ChannelProperties;
        updatedProperties: ChannelProperties;
    };
    notify_channel_list_received: {};
    notify_client_enter_view: {
        client: ClientEntry;
        reason: ViewReasonId;
        isServerJoin: boolean;
        targetChannel: ChannelEntry;
    };
    notify_client_moved: {
        client: ClientEntry;
        oldChannel: ChannelEntry | undefined;
        newChannel: ChannelEntry;
    };
    notify_client_leave_view: {
        client: ClientEntry;
        reason: ViewReasonId;
        message?: string;
        isServerLeave: boolean;
        sourceChannel: ChannelEntry;
    };
    notify_selected_entry_changed: {
        oldEntry: ChannelTreeEntry<any> | undefined;
        newEntry: ChannelTreeEntry<any> | undefined;
    };
}
export declare class ChannelTree {
    readonly events: Registry<ChannelTreeEvents>;
    client: ConnectionHandler;
    server: ServerEntry;
    channels: ChannelEntry[];
    clients: ClientEntry[];
    channelsInitialized: boolean;
    readonly popoutController: ChannelTreePopoutController;
    mainTreeUiEvents: Registry<ChannelTreeUIEvents>;
    private selectedEntry;
    private showQueries;
    private channelLast?;
    private channelFirst?;
    constructor(client: ConnectionHandler);
    channelsOrdered(): ChannelEntry[];
    findEntryId(entryId: number): ServerEntry | ChannelEntry | ClientEntry;
    getSelectedEntry(): ChannelTreeEntry<any> | undefined;
    setSelectedEntry(entry: ChannelTreeEntry<any> | undefined): void;
    destroy(): void;
    initialiseHead(serverName: string, address: ServerAddress): void;
    rootChannel(): ChannelEntry[];
    deleteChannel(channel: ChannelEntry): void;
    handleChannelCreated(previous: ChannelEntry, parent: ChannelEntry, channelId: number, channelName: string): ChannelEntry;
    findChannel(channelId: number): ChannelEntry | undefined;
    /**
     * Resolve a channel by its path
     */
    resolveChannelPath(target: string): ChannelEntry | undefined;
    find_channel_by_name(name: string, parent?: ChannelEntry, force_parent?: boolean): ChannelEntry | undefined;
    private unregisterChannelFromTree;
    moveChannel(channel: ChannelEntry, channelPrevious: ChannelEntry, parent: ChannelEntry, isInsertMove: boolean): void;
    deleteClient(client: ClientEntry, reason: {
        reason: ViewReasonId;
        message?: string;
        serverLeave: boolean;
    }): void;
    registerClient(client: ClientEntry): void;
    unregisterClient(client: ClientEntry): void;
    insertClient(client: ClientEntry, channel: ChannelEntry, reason: {
        reason: ViewReasonId;
        isServerJoin: boolean;
    }): ClientEntry;
    moveClient(client: ClientEntry, targetChannel: ChannelEntry): void;
    findClient?(clientId: number): ClientEntry;
    find_client_by_dbid?(client_dbid: number): ClientEntry;
    find_client_by_unique_id?(unique_id: string): ClientEntry;
    showContextMenu(x: number, y: number, on_close?: () => void): void;
    showMultiSelectContextMenu(entries: ChannelTreeEntry<any>[], x: number, y: number): void;
    clientsByGroup(group: Group): ClientEntry[];
    clientsByChannel(channel: ChannelEntry): ClientEntry[];
    reset(): void;
    spawnCreateChannel(parent?: ChannelEntry): void;
    toggle_server_queries(flag: boolean): void;
    areServerQueriesShown(): boolean;
    get_first_channel?(): ChannelEntry;
    unsubscribe_all_channels(): void;
    subscribe_all_channels(): void;
    expand_channels(root?: ChannelEntry): void;
    collapse_channels(root?: ChannelEntry): void;
}
