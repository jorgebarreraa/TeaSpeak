import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { Registry } from "tc-shared/events";
import { ChannelBarUiEvents } from "tc-shared/ui/frames/side/ChannelBarDefinitions";
import { ChannelConversationController } from "tc-shared/ui/frames/side/ChannelConversationController";
export declare class ChannelBarController {
    readonly uiEvents: Registry<ChannelBarUiEvents>;
    private channelConversations;
    private description;
    private fileBrowser;
    private currentConnection;
    private listenerConnection;
    private currentChannel;
    private listenerChannel;
    constructor();
    destroy(): void;
    getChannelConversationController(): ChannelConversationController;
    setConnectionHandler(handler: ConnectionHandler): void;
    private setChannel;
    private notifyChannelId;
    private notifyChannelMode;
    private notifyModeData;
}
