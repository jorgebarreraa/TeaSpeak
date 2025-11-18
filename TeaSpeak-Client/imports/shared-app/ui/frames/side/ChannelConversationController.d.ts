import { ConnectionHandler } from "../../../ConnectionHandler";
import { AbstractConversationController } from "./AbstractConversationController";
import { ChannelConversation, ChannelConversationEvents, ChannelConversationManager, ChannelConversationManagerEvents } from "tc-shared/conversations/ChannelConversationManager";
import { ChannelConversationUiEvents } from "tc-shared/ui/frames/side/ChannelConversationDefinitions";
export declare class ChannelConversationController extends AbstractConversationController<ChannelConversationUiEvents, ChannelConversationManager, ChannelConversationManagerEvents, ChannelConversation, ChannelConversationEvents> {
    private connection;
    private connectionListener;
    constructor();
    destroy(): void;
    setConnectionHandler(connection: ConnectionHandler): void;
    private handleMessageDelete;
    protected registerConversationEvents(conversation: ChannelConversation): void;
}
