import { PermissionType } from "../permission/PermissionType";
import { LaterPromise } from "../utils/LaterPromise";
import { ServerCommand } from "../connection/ConnectionBase";
import { CommandResult } from "../connection/ServerConnectionDeclaration";
import { ConnectionHandler } from "../ConnectionHandler";
import { AbstractCommandHandler } from "../connection/AbstractCommandHandler";
import { Registry } from "../events";
export declare class PermissionInfo {
    name: string;
    id: number;
    description: string;
    is_boolean(): boolean;
    id_grant(): number;
}
export declare class PermissionGroup {
    begin: number;
    end: number;
    deep: number;
    name: string;
}
export declare class GroupedPermissions {
    group: PermissionGroup;
    permissions: PermissionInfo[];
    children: GroupedPermissions[];
    parent: GroupedPermissions;
}
export declare class PermissionValue {
    readonly type: PermissionInfo;
    value: number | undefined;
    flag_skip: boolean;
    flag_negate: boolean;
    granted_value: number;
    constructor(type: any, value?: any);
    granted(requiredValue: number, required?: boolean): boolean;
    hasValue(): boolean;
    hasGrant(): boolean;
    valueOr(fallback: number): number;
    valueNormalOr(fallback: number): number;
}
export declare class NeededPermissionValue extends PermissionValue {
    constructor(type: any, value: any);
}
export declare type PermissionRequestKeys = {
    client_id?: number;
    channel_id?: number;
    playlist_id?: number;
};
export declare type PermissionRequest = PermissionRequestKeys & {
    timeout_id: any;
    promise: LaterPromise<PermissionValue[]>;
};
export declare namespace find {
    type Entry = {
        type: "server" | "channel" | "client" | "client_channel" | "channel_group" | "server_group";
        value: number;
        id: number;
    };
    type Client = Entry & {
        type: "client";
        client_id: number;
    };
    type Channel = Entry & {
        type: "channel";
        channel_id: number;
    };
    type Server = Entry & {
        type: "server";
    };
    type ClientChannel = Entry & {
        type: "client_channel";
        client_id: number;
        channel_id: number;
    };
    type ChannelGroup = Entry & {
        type: "channel_group";
        group_id: number;
    };
    type ServerGroup = Entry & {
        type: "server_group";
        group_id: number;
    };
}
export interface PermissionManagerEvents {
    client_permissions_changed: {};
}
export declare type RequestLists = "requests_channel_permissions" | "requests_client_permissions" | "requests_client_channel_permissions" | "requests_playlist_permissions" | "requests_playlist_client_permissions";
export declare class PermissionManager extends AbstractCommandHandler {
    readonly events: Registry<PermissionManagerEvents>;
    readonly handle: ConnectionHandler;
    permissionList: PermissionInfo[];
    permissionGroups: PermissionGroup[];
    neededPermissions: NeededPermissionValue[];
    needed_permission_change_listener: {
        [permission: string]: ((value?: PermissionValue) => void)[];
    };
    requests_channel_permissions: PermissionRequest[];
    requests_client_permissions: PermissionRequest[];
    requests_client_channel_permissions: PermissionRequest[];
    requests_playlist_permissions: PermissionRequest[];
    requests_playlist_client_permissions: PermissionRequest[];
    requests_permfind: {
        timeout_id: number;
        permission: string;
        callback: (status: "success" | "error", data: any) => void;
    }[];
    initializedListener: ((initialized: boolean) => void)[];
    private cacheNeededPermissions;
    static readonly group_mapping: {
        name: string;
        deep: number;
    }[];
    private _group_mapping;
    static parse_permission_bulk(json: any[], manager: PermissionManager): PermissionValue[];
    constructor(client: ConnectionHandler);
    destroy(): void;
    handle_command(command: ServerCommand): boolean;
    initialized(): boolean;
    requestPermissionList(): void;
    private onPermissionList;
    private onNeededPermissions;
    register_needed_permission(key: PermissionType, listener: () => any): () => void;
    unregister_needed_permission(key: PermissionType, listener: () => any): void;
    resolveInfo?(key: number | string | PermissionType): PermissionInfo;
    private onChannelPermList;
    private execute_channel_permission_request;
    requestChannelPermissions(channelId: number, processResult?: boolean): Promise<PermissionValue[]>;
    private onClientPermList;
    private execute_client_permission_request;
    requestClientPermissions(client_id: number): Promise<PermissionValue[]>;
    private onChannelClientPermList;
    private execute_client_channel_permission_request;
    requestClientChannelPermissions(client_id: number, channel_id: number): Promise<PermissionValue[]>;
    private onPlaylistPermList;
    private execute_playlist_permission_request;
    requestPlaylistPermissions(playlist_id: number): Promise<PermissionValue[]>;
    private onPlaylistClientPermList;
    private execute_playlist_client_permission_request;
    requestPlaylistClientPermissions(playlist_id: number, client_database_id: number): Promise<PermissionValue[]>;
    private readonly criteria_equal;
    private execute_permission_request;
    private fullfill_permission_request;
    find_permission(...permissions: string[]): Promise<find.Entry[]>;
    neededPermission(key: number | string | PermissionType | PermissionInfo): NeededPermissionValue;
    groupedPermissions(): GroupedPermissions[];
    /**
     * Generates an enum with all know permission types, used for the enum above
     */
    export_permission_types(): string;
    getFailedPermission(command: CommandResult, index?: number): any;
}
