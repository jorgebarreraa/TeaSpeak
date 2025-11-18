import { MessageCommand, MessageCommandResult, MessageNotify } from "./Messages";
import { Registry } from "tc-events";
export declare const kApiVersion = 1;
declare type ConnectionState = "disconnected" | "connecting" | "connected" | "reconnect-pending";
interface ClientServiceConnectionEvents {
    notify_state_changed: {
        oldState: ConnectionState;
        newState: ConnectionState;
    };
}
declare type NotifyPayloadType<K extends MessageNotify["type"]> = Extract<MessageNotify, {
    type: K;
}>["payload"];
declare type CommandPayloadType<K extends MessageCommand["type"]> = Extract<MessageCommand, {
    type: K;
}>["payload"];
export declare class ClientServiceConnection {
    readonly events: Registry<ClientServiceConnectionEvents>;
    readonly reconnectInterval: number;
    private readonly serviceHost;
    private reconnectTimeout;
    private connectionState;
    private connection;
    private pendingCommands;
    private notifyHandler;
    constructor(serviceHost: string, reconnectInterval: number);
    destroy(): void;
    getState(): ConnectionState;
    private setState;
    connect(): void;
    disconnect(): void;
    cancelReconnect(): void;
    executeMessageCommand(command: MessageCommand): Promise<MessageCommandResult>;
    executeCommand<K extends MessageCommand["type"]>(command: K, payload: CommandPayloadType<K>): Promise<MessageCommandResult>;
    registerNotifyHandler<K extends MessageNotify["type"]>(notify: K, callback: (notify: NotifyPayloadType<K>) => void): () => void;
    unregisterNotifyHandler<K extends MessageNotify["type"]>(callback: (notify: NotifyPayloadType<K>) => void): any;
    unregisterNotifyHandler<K extends MessageNotify["type"]>(notify: K, callback: (notify: NotifyPayloadType<K>) => void): any;
    catchNotify<K extends MessageNotify["type"]>(notify: K, filter?: (value: NotifyPayloadType<K>) => boolean): () => ({
        status: "success";
        value: NotifyPayloadType<K>;
    } | {
        status: "fail";
    });
    private handleConnectFail;
    private handleConnectionLost;
    private executeReconnect;
    private handleServerMessage;
}
export {};
