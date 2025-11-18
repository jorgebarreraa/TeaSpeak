import { ChannelTree } from "./ChannelTree";
import * as contextmenu from "../ui/elements/ContextMenu";
import { Registry } from "../events";
import { ChannelTreeEntry, ChannelTreeEntryEvents } from "./ChannelTreeEntry";
import { HostBannerInfo } from "tc-shared/ui/frames/HostBannerDefinitions";
import { ServerAudioEncryptionMode, ServerConnectionInfo, ServerConnectionInfoResult, ServerProperties } from "tc-shared/tree/ServerDefinitions";
export * from "./ServerDefinitions";
export interface ServerAddress {
    host: string;
    port: number;
}
export declare function parseServerAddress(address: string): ServerAddress | undefined;
export declare function stringifyServerAddress(address: ServerAddress): string;
export interface ServerEvents extends ChannelTreeEntryEvents {
    notify_properties_updated: {
        updated_properties: Partial<ServerProperties>;
        server_properties: ServerProperties;
    };
    notify_host_banner_updated: {};
}
export declare class ServerEntry extends ChannelTreeEntry<ServerEvents> {
    remote_address: ServerAddress;
    channelTree: ChannelTree;
    properties: ServerProperties;
    readonly events: Registry<ServerEvents>;
    private info_request_promise;
    private info_request_promise_resolve;
    private info_request_promise_reject;
    private requestInfoPromise;
    private requestInfoPromiseTimestamp;
    private _info_connection_promise;
    private _info_connection_promise_timestamp;
    private _info_connection_promise_resolve;
    private _info_connection_promise_reject;
    lastInfoRequest: number;
    nextInfoRequest: number;
    private _destroyed;
    constructor(tree: any, name: any, address: ServerAddress);
    destroy(): void;
    contextMenuItems(): contextmenu.MenuEntry[];
    showContextMenu(x: number, y: number, on_close?: () => void): void;
    updateVariables(is_self_notify: boolean, ...variables: {
        key: string;
        value: string;
    }[]): void;
    updateProperties(): Promise<void>;
    request_connection_info(): Promise<ServerConnectionInfo>;
    requestConnectionInfo(): Promise<ServerConnectionInfoResult>;
    private doRequestConnectionInfo;
    set_connection_info(info: ServerConnectionInfo): void;
    shouldUpdateProperties(): boolean;
    calculateUptime(): number;
    reset(): void;
    generateHostBannerInfo(): HostBannerInfo;
    getAudioEncryptionMode(): ServerAudioEncryptionMode;
}
