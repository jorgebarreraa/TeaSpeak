import { ChannelTree } from "tc-shared/tree/ChannelTree";
export declare class ChannelTreePopoutController {
    readonly channelTree: ChannelTree;
    private popoutInstance;
    private uiEvents;
    private treeEvents;
    private controlBarEvents;
    private generalEvents;
    constructor(channelTree: ChannelTree);
    destroy(): void;
    hasBeenPopedOut(): boolean;
    popout(): void;
    popin(): void;
    private sendTitle;
}
