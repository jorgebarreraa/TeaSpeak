import { LaterPromise } from "../utils/LaterPromise";
import { PermissionValue } from "../permission/PermissionManager";
import { ServerCommand } from "../connection/ConnectionBase";
import { ConnectionHandler } from "../ConnectionHandler";
import { AbstractCommandHandler } from "../connection/AbstractCommandHandler";
import { Registry } from "../events";
export declare enum GroupType {
    QUERY = 0,
    TEMPLATE = 1,
    NORMAL = 2
}
export declare enum GroupTarget {
    SERVER = 0,
    CHANNEL = 1
}
export declare class GroupProperties {
    iconid: number;
    sortid: number;
    savedb: boolean;
    namemode: number;
}
export declare class GroupPermissionRequest {
    group_id: number;
    promise: LaterPromise<PermissionValue[]>;
}
export declare type GroupUpdate = {
    key: GroupProperty;
    group: Group;
    oldValue: any;
    newValue: any;
};
export interface GroupManagerEvents {
    notify_reset: {};
    notify_groups_created: {
        groups: Group[];
        cause: "list-update" | "initialize" | "user-action";
    };
    notify_groups_deleted: {
        groups: Group[];
        cause: "list-update" | "reset" | "user-action";
    };
    notify_groups_updated: {
        updates: GroupUpdate[];
    };
    notify_groups_received: {};
}
export declare type GroupProperty = "name" | "icon" | "sort-id" | "save-db" | "name-mode";
export interface GroupEvents {
    notify_group_deleted: {};
    notify_properties_updated: {
        updated_properties: GroupProperty[];
    };
    notify_needed_powers_updated: {};
}
export declare class Group {
    readonly handle: GroupManager;
    readonly events: Registry<GroupEvents>;
    readonly properties: GroupProperties;
    readonly id: number;
    readonly target: GroupTarget;
    readonly type: GroupType;
    name: string;
    requiredModifyPower: number;
    requiredMemberAddPower: number;
    requiredMemberRemovePower: number;
    constructor(handle: GroupManager, id: number, target: GroupTarget, type: GroupType, name: string);
    updatePropertiesFromGroupList(data: any): GroupUpdate[];
}
export declare class GroupManager extends AbstractCommandHandler {
    static sorter(): (a: Group, b: Group) => number;
    readonly events: Registry<GroupManagerEvents>;
    readonly connectionHandler: ConnectionHandler;
    serverGroups: Group[];
    channelGroups: Group[];
    private allGroupsReceived;
    private readonly connectionStateListener;
    private groupPermissionRequests;
    constructor(client: ConnectionHandler);
    destroy(): void;
    reset(): void;
    handle_command(command: ServerCommand): boolean;
    requestGroups(): void;
    findServerGroup(id: number): Group | undefined;
    findChannelGroup(id: number): Group | undefined;
    private handleGroupList;
    request_permissions(group: Group): Promise<PermissionValue[]>;
    private handleGroupPermissionList;
}
