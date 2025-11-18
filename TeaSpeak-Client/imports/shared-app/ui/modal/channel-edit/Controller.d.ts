import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { ChannelEntry, ChannelProperties } from "tc-shared/tree/Channel";
import PermissionType from "tc-shared/permission/PermissionType";
export declare type ChannelEditCallback = (properties: Partial<ChannelProperties>, permissions: ChannelEditChangedPermission[]) => void;
export declare type ChannelEditChangedPermission = {
    permission: PermissionType;
    value: number;
};
export declare const spawnChannelEditNew: (connection: ConnectionHandler, channel: ChannelEntry | undefined, parent: ChannelEntry | undefined, callback: ChannelEditCallback) => void;
