import { ChannelEditableProperty } from "tc-shared/ui/modal/channel-edit/Definitions";
import { ChannelEntry, ChannelProperties } from "tc-shared/tree/Channel";
import { ChannelTree } from "tc-shared/tree/ChannelTree";
import { PermissionManager } from "tc-shared/permission/PermissionManager";
export declare const ChannelPropertyValidators: {
    [T in keyof ChannelEditableProperty]?: (currentProperties: ChannelProperties, originalProperties: ChannelProperties, channel: ChannelEntry | undefined, parent: ChannelEntry | undefined, permissions: PermissionManager, channelTree: ChannelTree) => boolean;
};
