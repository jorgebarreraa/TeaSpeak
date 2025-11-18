import "broadcastchannel-polyfill";
import { ConnectHandler } from "../ipc/ConnectHandler";
interface IpcRawMessage {
    timestampSend: number;
    sourcePeerId: string;
    targetPeerId: string;
    targetChannelId: string;
    message: ChannelMessage;
}
export interface ChannelMessage {
    type: string;
    data: any;
}
export declare abstract class BasicIPCHandler {
    protected static readonly BROADCAST_UNIQUE_ID = "00000000-0000-4000-0000-000000000000";
    protected readonly applicationChannelId: string;
    protected readonly localPeerId: string;
    protected registeredChannels: IPCChannel[];
    protected constructor(applicationChannelId: string);
    setup(): void;
    getApplicationChannelId(): string;
    getLocalPeerId(): string;
    abstract sendMessage(message: IpcRawMessage): any;
    protected handleMessage(message: IpcRawMessage): void;
    /**
     * @param channelId
     * @param remotePeerId The peer to receive messages from. If empty messages will be broadcasted
     */
    createChannel(channelId: string, remotePeerId?: string): IPCChannel;
    /**
     * Create a channel which only communicates with the TeaSpeak - Core.
     * @param channelId
     */
    createCoreControlChannel(channelId: string): IPCChannel;
    channels(): IPCChannel[];
    deleteChannel(channel: IPCChannel): void;
}
export interface IPCChannel {
    /** Channel id */
    readonly channelId: string;
    /** Target peer id. If set only messages from that process will be processed */
    targetPeerId?: string;
    messageHandler: (sourcePeerId: string, broadcast: boolean, message: ChannelMessage) => void;
    sendMessage(type: string, data: any, remotePeerId?: string): any;
}
export declare function setupIpcHandler(): void;
export declare function getIpcInstance(): BasicIPCHandler;
export declare function getInstanceConnectHandler(): ConnectHandler;
export {};
