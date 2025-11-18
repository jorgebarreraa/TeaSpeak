import { AbstractConversationUiEvents } from "./AbstractConversationDefinitions";
import { Registry } from "../../../events";
import { AbstractChat, AbstractChatManager, AbstractChatManagerEvents, AbstractConversationEvents } from "tc-shared/conversations/AbstractConversion";
export declare const kMaxChatFrameMessageSize = 50;
export declare abstract class AbstractConversationController<Events extends AbstractConversationUiEvents, Manager extends AbstractChatManager<ManagerEvents, ConversationType, ConversationEvents>, ManagerEvents extends AbstractChatManagerEvents<ConversationType>, ConversationType extends AbstractChat<ConversationEvents>, ConversationEvents extends AbstractConversationEvents> {
    protected readonly uiEvents: Registry<Events>;
    protected conversationManager: Manager | undefined;
    protected listenerManager: (() => void)[];
    protected currentSelectedConversation: ConversationType;
    protected currentSelectedListener: (() => void)[];
    protected constructor();
    destroy(): void;
    getUiEvents(): Registry<Events>;
    protected setConversationManager(manager: Manager | undefined): void;
    protected registerConversationManagerEvents(manager: Manager): void;
    protected registerConversationEvents(conversation: ConversationType): void;
    protected setCurrentlySelected(conversation: ConversationType | undefined): void;
    protected reportStateToUI(conversation: AbstractChat<any>): void;
    uiQueryHistory(conversation: AbstractChat<any>, timestamp: number, enforce?: boolean): void;
    protected getCurrentConversation(): ConversationType | undefined;
    protected handleQueryConversationState(event: AbstractConversationUiEvents["query_conversation_state"]): void;
    protected handleQueryHistory(event: AbstractConversationUiEvents["query_conversation_history"]): void;
    protected handleClearUnreadFlag(event: AbstractConversationUiEvents["action_clear_unread_flag" | "action_self_typing"]): void;
    protected handleSendMessage(event: AbstractConversationUiEvents["action_send_message"]): void;
    protected handleJumpToPresent(event: AbstractConversationUiEvents["action_jump_to_present"]): void;
    private handleQuerySelectedChat;
    private handleActionSelectChat;
}
