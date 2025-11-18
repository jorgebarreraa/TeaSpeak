import { ConnectionHandler } from "../../../ConnectionHandler";
import { PrivateConversationUIEvents } from "../../../ui/frames/side/PrivateConversationDefinitions";
import { AbstractConversationUiEvents } from "./AbstractConversationDefinitions";
import { AbstractConversationController } from "./AbstractConversationController";
import { PrivateConversation, PrivateConversationEvents, PrivateConversationManager, PrivateConversationManagerEvents } from "tc-shared/conversations/PrivateConversationManager";
export declare type OutOfViewClient = {
    nickname: string;
    clientId: number;
    uniqueId: string;
};
export declare class PrivateConversationController extends AbstractConversationController<PrivateConversationUIEvents, PrivateConversationManager, PrivateConversationManagerEvents, PrivateConversation, PrivateConversationEvents> {
    private connection;
    private connectionListener;
    private listenerConversation;
    constructor();
    destroy(): void;
    setConnectionHandler(connection: ConnectionHandler): void;
    protected registerConversationManagerEvents(manager: PrivateConversationManager): void;
    focusInput(): void;
    private reportConversationList;
    private handleQueryPrivateConversations;
    private handleConversationClose;
    protected handleActionSelfTyping1(_event: AbstractConversationUiEvents["action_self_typing"]): void;
}
