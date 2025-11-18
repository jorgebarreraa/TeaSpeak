import { ChatEvent, ChatEventMessage, ChatMessage, ChatState, ConversationHistoryResponse } from "../ui/frames/side/AbstractConversationDefinitions";
import { Registry } from "tc-shared/events";
import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { ChannelConversationMode } from "tc-shared/tree/Channel";
export declare const kMaxChatFrameMessageSize = 50;
export interface AbstractConversationEvents {
    notify_chat_event: {
        triggerUnread: boolean;
        event: ChatEvent;
    };
    notify_unread_timestamp_changed: {
        timestamp: number;
    };
    notify_unread_state_changed: {
        unread: boolean;
    };
    notify_send_toggle: {
        enabled: boolean;
    };
    notify_state_changed: {
        oldSTate: ChatState;
        newState: ChatState;
    };
    notify_history_state_changed: {
        hasHistory: boolean;
    };
    notify_conversation_mode_changed: {
        newMode: ChannelConversationMode;
    };
    notify_read_state_changed: {
        readable: boolean;
    };
}
export declare abstract class AbstractChat<Events extends AbstractConversationEvents> {
    readonly events: Registry<Events>;
    protected readonly connection: ConnectionHandler;
    protected readonly chatId: string;
    protected presentMessages: ChatEvent[];
    protected presentEvents: Exclude<ChatEvent, ChatEventMessage>[];
    private mode;
    protected failedPermission: string;
    protected errorMessage: string;
    private conversationMode;
    protected unreadTimestamp: number;
    protected unreadState: boolean;
    protected messageSendEnabled: boolean;
    private conversationReadable;
    private history;
    protected constructor(connection: ConnectionHandler, chatId: string);
    destroy(): void;
    getCurrentMode(): ChatState;
    protected setCurrentMode(mode: ChatState): void;
    registerChatEvent(event: ChatEvent, triggerUnread: boolean): void;
    protected registerIncomingMessage(message: ChatMessage, isOwnMessage: boolean, uniqueId: string): void;
    protected doSendMessage(message: string, targetMode: number, target: number): Promise<boolean>;
    getChatId(): string;
    isUnread(): boolean;
    getConversationMode(): ChannelConversationMode;
    isPrivate(): boolean;
    protected setConversationMode(mode: ChannelConversationMode, logChange: boolean): void;
    isReadable(): boolean;
    protected setReadable(flag: boolean): void;
    isSendEnabled(): boolean;
    getUnreadTimestamp(): number | undefined;
    getPresentMessages(): ChatEvent[];
    getPresentEvents(): ChatEvent[];
    getErrorMessage(): string | undefined;
    getFailedPermission(): string | undefined;
    setUnreadTimestamp(timestamp: number): void;
    protected updateUnreadState(): void;
    hasHistory(): boolean;
    protected setHistory(hasHistory: boolean): void;
    protected lastEvent(): ChatEvent | undefined;
    protected sendMessageSendingEnabled(enabled: boolean): void;
    abstract queryHistory(criteria: {
        begin?: number;
        end?: number;
        limit?: number;
    }): Promise<ConversationHistoryResponse>;
    abstract queryCurrentMessages(): any;
    abstract sendMessage(text: string): any;
}
export interface AbstractChatManagerEvents<ConversationType> {
    notify_selected_changed: {
        oldConversation: ConversationType;
        newConversation: ConversationType;
    };
    notify_conversation_destroyed: {
        conversation: ConversationType;
    };
    notify_conversation_created: {
        conversation: ConversationType;
    };
    notify_unread_count_changed: {
        unreadConversations: number;
    };
    notify_cross_conversation_support_changed: {
        crossConversationSupported: boolean;
    };
}
export declare abstract class AbstractChatManager<ManagerEvents extends AbstractChatManagerEvents<ConversationType>, ConversationType extends AbstractChat<ConversationEvents>, ConversationEvents extends AbstractConversationEvents> {
    readonly events: Registry<ManagerEvents>;
    readonly connection: ConnectionHandler;
    protected readonly listenerConnection: (() => void)[];
    private readonly listenerUnreadTimestamp;
    private conversations;
    private selectedConversation;
    private currentUnreadCount;
    private crossConversationSupport;
    historyUiStates: {
        [id: string]: {
            executingUIHistoryQuery: boolean;
            historyErrorMessage: string | undefined;
            historyRetryTimestamp: number;
        };
    };
    protected constructor(connection: ConnectionHandler);
    destroy(): void;
    getConversations(): ConversationType[];
    getUnreadCount(): number;
    hasCrossConversationSupport(): boolean;
    getSelectedConversation(): ConversationType;
    setSelectedConversation(conversation: ConversationType | undefined): void;
    findConversationById(id: string): ConversationType;
    protected registerConversation(conversation: ConversationType): void;
    protected unregisterConversation(conversation: ConversationType): void;
    protected setCrossConversationSupport(supported: boolean): void;
}
