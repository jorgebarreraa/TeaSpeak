import { ChannelPropertyPermission } from "tc-shared/ui/modal/channel-edit/Definitions";
import { PermissionManager } from "tc-shared/permission/PermissionManager";
import { ChannelEntry } from "tc-shared/tree/Channel";
import { ChannelTree } from "tc-shared/tree/ChannelTree";
export declare type ChannelPropertyPermissionsProvider<T extends keyof ChannelPropertyPermission> = {
    provider: (permissions: PermissionManager, channel: ChannelEntry | undefined, channelTree: ChannelTree) => ChannelPropertyPermission[T];
    registerUpdates: (callback: () => void, permissions: PermissionManager, channel: ChannelEntry | undefined, channelTree: ChannelTree) => (() => void)[];
};
export declare const ChannelPropertyPermissionsProviders: {
    [T in keyof ChannelPropertyPermission]?: ChannelPropertyPermissionsProvider<T>;
};
