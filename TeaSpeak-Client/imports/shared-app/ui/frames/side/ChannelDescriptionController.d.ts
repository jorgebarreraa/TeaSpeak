import { ChannelEntry } from "tc-shared/tree/Channel";
import { Registry } from "tc-shared/events";
import { ChannelDescriptionUiEvents } from "tc-shared/ui/frames/side/ChannelDescriptionDefinitions";
export declare class ChannelDescriptionController {
    readonly uiEvents: Registry<ChannelDescriptionUiEvents>;
    private currentChannel;
    private listenerChannel;
    private descriptionSendPending;
    private cachedDescriptionStatus;
    private cachedDescriptionAge;
    constructor();
    destroy(): void;
    setChannel(channel: ChannelEntry): void;
    private notifyDescription;
    private updateCachedDescriptionStatus;
}
