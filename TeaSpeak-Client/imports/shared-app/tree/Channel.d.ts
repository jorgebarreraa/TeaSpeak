import { ChannelTree } from "./ChannelTree";
import { ClientEntry } from "./Client";
import { PermissionType } from "../permission/PermissionType";
import { Registry } from "../events";
import { ChannelTreeEntry, ChannelTreeEntryEvents } from "./ChannelTreeEntry";
import { ClientIcon } from "svg-sprites/client-icons";
import { EventChannelData } from "tc-shared/connectionlog/Definitions";
import { ChannelDescriptionResult } from "tc-shared/tree/ChannelDefinitions";
export declare enum ChannelType {
    PERMANENT = 0,
    SEMI_PERMANENT = 1,
    TEMPORARY = 2
}
export declare namespace ChannelType {
    function normalize(mode: ChannelType): string;
}
export declare enum ChannelSubscribeMode {
    SUBSCRIBED = 0,
    UNSUBSCRIBED = 1,
    INHERITED = 2
}
export declare enum ChannelConversationMode {
    Public = 0,
    Private = 1,
    None = 2
}
export declare enum ChannelSidebarMode {
    Conversation = 0,
    Description = 1,
    FileTransfer = 2,
    Unknown = 255
}
export declare class ChannelProperties {
    channel_order: number;
    channel_name: string;
    channel_name_phonetic: string;
    channel_topic: string;
    channel_password: string;
    channel_codec: number;
    channel_codec_quality: number;
    channel_codec_is_unencrypted: boolean;
    channel_maxclients: number;
    channel_maxfamilyclients: number;
    channel_needed_talk_power: number;
    channel_flag_permanent: boolean;
    channel_flag_semi_permanent: boolean;
    channel_flag_default: boolean;
    channel_flag_password: boolean;
    channel_flag_maxclients_unlimited: boolean;
    channel_flag_maxfamilyclients_inherited: boolean;
    channel_flag_maxfamilyclients_unlimited: boolean;
    channel_icon_id: number;
    channel_delete_delay: number;
    channel_description: string;
    channel_conversation_mode: ChannelConversationMode;
    channel_conversation_history_length: number;
    channel_sidebar_mode: ChannelSidebarMode;
}
export interface ChannelEvents extends ChannelTreeEntryEvents {
    notify_properties_updated: {
        updated_properties: {
            [Key in keyof ChannelProperties]: ChannelProperties[Key];
        };
        channel_properties: ChannelProperties;
    };
    notify_cached_password_updated: {
        reason: "channel-password-changed" | "password-miss-match" | "password-entered";
        new_hash?: string;
    };
    notify_subscribe_state_changed: {
        channel_subscribed: boolean;
    };
    notify_collapsed_state_changed: {
        collapsed: boolean;
    };
    notify_description_changed: {};
}
export declare type ChannelNameAlignment = "center" | "right" | "left" | "normal" | "repetitive";
export declare class ChannelNameParser {
    readonly originalName: string;
    alignment: ChannelNameAlignment;
    text: string;
    uniqueId: string;
    constructor(name: string, hasParentChannel: boolean);
    private parse;
}
export declare class ChannelEntry extends ChannelTreeEntry<ChannelEvents> {
    channelTree: ChannelTree;
    channelId: number;
    parent?: ChannelEntry;
    properties: ChannelProperties;
    channel_previous?: ChannelEntry;
    channel_next?: ChannelEntry;
    child_channel_head?: ChannelEntry;
    readonly events: Registry<ChannelEvents>;
    parsed_channel_name: ChannelNameParser;
    private _family_index;
    private _destroyed;
    private cachedPasswordHash;
    private channelDescriptionCacheTimestamp;
    private channelDescriptionCallback;
    private channelDescriptionPromise;
    private collapsed;
    private subscribed;
    private subscriptionMode;
    private clientList;
    private readonly clientPropertyChangedListener;
    constructor(channelTree: ChannelTree, channelId: number, channelName: string);
    destroy(): void;
    channelName(): string;
    channelDepth(): number;
    formattedChannelName(): string;
    clearDescriptionCache(): void;
    getChannelDescription(ignoreCache: boolean): Promise<ChannelDescriptionResult>;
    private doGetChannelDescriptionNew;
    isDescriptionCached(): boolean;
    registerClient(client: ClientEntry): void;
    unregisterClient(client: ClientEntry, noEvent?: boolean): void;
    private reorderClientList;
    parent_channel(): ChannelEntry;
    hasParent(): boolean;
    getChannelId(): number;
    children(deep?: boolean): ChannelEntry[];
    clients(deep?: boolean): ClientEntry[];
    channelClientsOrdered(): ClientEntry[];
    calculate_family_index(enforce_recalculate?: boolean): number;
    showContextMenu(x: number, y: number, on_close?: () => void): void;
    updateVariables(...variables: {
        key: string;
        value: string;
    }[]): void;
    generateBBCode(): string;
    getChannelType(): ChannelType;
    joinChannel(ignorePasswordFlag?: boolean): Promise<boolean>;
    requestChannelPassword(ignorePermission: PermissionType): Promise<{
        hash: string;
    } | undefined>;
    invalidateCachedPassword(): void;
    setCachedHashedPassword(passwordHash: string): void;
    getCachedPasswordHash(): string;
    updateSubscribeMode(): Promise<void>;
    subscribe(): Promise<void>;
    unsubscribe(inherited_subscription_mode?: boolean): Promise<void>;
    isCollapsed(): boolean;
    setCollapsed(flag: boolean): void;
    isSubscribed(): boolean;
    setSubscribed(flag: boolean): void;
    getSubscriptionMode(): ChannelSubscribeMode;
    setSubscriptionMode(mode: ChannelSubscribeMode, dontSyncSubscribeMode?: boolean): void;
    log_data(): EventChannelData;
    getStatusIcon(): ClientIcon | undefined;
    handleDescriptionChanged(): void;
}
