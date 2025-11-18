import { ChannelEntry, ChannelProperties } from "tc-shared/tree/Channel";
import { ChannelEditableProperty } from "tc-shared/ui/modal/channel-edit/Definitions";
import { ChannelTree } from "tc-shared/tree/ChannelTree";
export declare type ChannelPropertyProvider<T extends keyof ChannelEditableProperty> = {
    provider: (properties: ChannelProperties, channel: ChannelEntry | undefined, parentChannel: ChannelEntry | undefined, channelTree: ChannelTree) => Promise<ChannelEditableProperty[T]>;
    applier: (value: ChannelEditableProperty[T], properties: Partial<ChannelProperties>, channel: ChannelEntry | undefined) => void;
};
export declare const ChannelPropertyProviders: {
    [T in keyof ChannelEditableProperty]?: ChannelPropertyProvider<T>;
};
