import { AbstractChat, AbstractConversationEvents, AbstractChatManager, AbstractChatManagerEvents } from "./AbstractConversion";
import { ChatMessage, ConversationHistoryResponse } from "../ui/frames/side/AbstractConversationDefinitions";
import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { ChannelConversationMode } from "tc-shared/tree/Channel";
export interface ChannelConversationEvents extends AbstractConversationEvents {
    notify_messages_deleted: {
        messages: string[];
    };
    notify_messages_loaded: {};
}
export declare class ChannelConversation extends AbstractChat<ChannelConversationEvents> {
    private readonly handle;
    readonly conversationId: number;
    private conversationVolatile;
    private preventUnreadUpdate;
    private executingHistoryQueries;
    private pendingHistoryQueries;
    historyQueryResponse: ChatMessage[];
    constructor(handle: ChannelConversationManager, id: number);
    destroy(): void;
    queryHistory(criteria: {
        begin?: number;
        end?: number;
        limit?: number;
    }): Promise<ConversationHistoryResponse>;
    queryCurrentMessages(): void;
    canClientAccessChat(): boolean;
    private executeHistoryQuery;
    updateIndexFromServer(info: any): void;
    handleIncomingMessage(message: ChatMessage, isOwnMessage: boolean): void;
    handleDeleteMessages(criteria: {
        begin: number;
        end: number;
        cldbid: number;
        limit: number;
    }): void;
    deleteMessage(messageUniqueId: string): void;
    setUnreadTimestamp(timestamp: number): void;
    setConversationMode(mode: ChannelConversationMode, logChange: boolean): void;
    localClientSwitchedChannel(type: "join" | "leave"): void;
    sendMessage(text: string): void;
    updateAccessState(): void;
}
export interface ChannelConversationManagerEvents extends AbstractChatManagerEvents<ChannelConversation> {
}
export declare class ChannelConversationManager extends AbstractChatManager<ChannelConversationManagerEvents, ChannelConversation, ChannelConversationEvents> {
    readonly connection: ConnectionHandler;
    constructor(connection: ConnectionHandler);
    destroy(): void;
    findConversation(channelId: number): ChannelConversation;
    findOrCreateConversation(channelId: number): ChannelConversation;
    destroyConversation(id: number): void;
    queryUnreadFlags(): void;
    private handleConversationHistory;
    private handleConversationIndex;
    private handleConversationMessageDelete;
}
