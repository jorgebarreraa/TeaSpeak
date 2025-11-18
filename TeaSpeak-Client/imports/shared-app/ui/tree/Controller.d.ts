import { ChannelTree } from "tc-shared/tree/ChannelTree";
import { Registry } from "tc-shared/events";
import { ChannelTreeUIEvents } from "tc-shared/ui/tree/Definitions";
export interface ChannelTreeRendererOptions {
    popoutButton: boolean;
}
export declare function initializeChannelTreeUiEvents(channelTree: ChannelTree, options: ChannelTreeRendererOptions): Registry<ChannelTreeUIEvents>;
export declare function initializeChannelTreeController(events: Registry<ChannelTreeUIEvents>, channelTree: ChannelTree, options: ChannelTreeRendererOptions): void;
