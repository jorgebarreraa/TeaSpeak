import { AbstractChat, AbstractConversationEvents, AbstractChatManager, AbstractChatManagerEvents } from "tc-shared/conversations/AbstractConversion";
import { ClientEntry } from "tc-shared/tree/Client";
import { ChatEvent, ChatMessage, ConversationHistoryResponse } from "../ui/frames/side/AbstractConversationDefinitions";
import { ChannelTreeEvents } from "tc-shared/tree/ChannelTree";
import { ConnectionHandler } from "tc-shared/ConnectionHandler";
export declare type OutOfViewClient = {
    nickname: string;
    clientId: number;
    uniqueId: string;
};
export interface PrivateConversationEvents extends AbstractConversationEvents {
    notify_partner_typing: {};
    notify_partner_changed: {
        chatId: string;
        clientId: number;
        name: string;
    };
    notify_partner_name_changed: {
        chatId: string;
        name: string;
    };
}
export declare class PrivateConversation extends AbstractChat<PrivateConversationEvents> {
    readonly clientUniqueId: string;
    private activeClientListener;
    private activeClient;
    private lastClientInfo;
    private conversationOpen;
    constructor(manager: PrivateConversationManager, client: ClientEntry | OutOfViewClient);
    destroy(): void;
    getActiveClient(): ClientEntry | OutOfViewClient | undefined;
    currentClientId(): number;
    getLastClientInfo(): OutOfViewClient;
    setActiveClientEntry(client: ClientEntry | OutOfViewClient | undefined): void;
    hasUnreadMessages(): boolean;
    handleIncomingMessage(client: ClientEntry | OutOfViewClient, isOwnMessage: boolean, message: ChatMessage): void;
    handleChatRemotelyClosed(clientId: number): void;
    handleClientEnteredView(client: ClientEntry, mode: "server-join" | "local-reconnect" | "appear"): void;
    handleRemoteComposing(_clientId: number): void;
    sendMessage(text: string): void;
    sendChatClose(): void;
    handleEventLeftView(event: ChannelTreeEvents["notify_client_leave_view"]): void;
    private registerClientEvents;
    private unregisterClientEvents;
    private updateClientInfo;
    setUnreadTimestamp(timestamp: number): void;
    canClientAccessChat(): boolean;
    handleLocalClientDisconnect(explicitDisconnect: boolean): void;
    queryCurrentMessages(): void;
    registerChatEvent(event: ChatEvent, triggerUnread: boolean): void;
    queryHistory(criteria: {
        begin?: number;
        end?: number;
        limit?: number;
    }): Promise<ConversationHistoryResponse>;
}
export interface PrivateConversationManagerEvents extends AbstractChatManagerEvents<PrivateConversation> {
}
export declare class PrivateConversationManager extends AbstractChatManager<PrivateConversationManagerEvents, PrivateConversation, PrivateConversationEvents> {
    readonly connection: ConnectionHandler;
    private channelTreeInitialized;
    constructor(connection: ConnectionHandler);
    destroy(): void;
    findConversation(client: ClientEntry | string): PrivateConversation;
    findOrCreateConversation(client: ClientEntry | OutOfViewClient): PrivateConversation;
    closeConversation(...conversations: PrivateConversation[]): void;
}
