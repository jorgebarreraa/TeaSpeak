import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { SideHeaderEvents } from "tc-shared/ui/frames/side/HeaderDefinitions";
import { Registry } from "tc-shared/events";
export declare class SideHeaderController {
    readonly uiEvents: Registry<SideHeaderEvents>;
    private connection;
    private listenerConnection;
    private listenerVoiceChannel;
    private listenerTextChannel;
    private currentVoiceChannel;
    private currentTextChannel;
    private pingUpdateInterval;
    constructor();
    private initialize;
    private initializeConnection;
    setConnectionHandler(connection: ConnectionHandler): void;
    getConnectionHandler(): ConnectionHandler | undefined;
    destroy(): void;
    private sendChannelState;
    private updateVoiceChannel;
    private updateTextChannel;
    private sendPing;
    private sendPrivateConversationInfo;
    private sendClientInfoOwnClient;
    private sendServerInfo;
}
