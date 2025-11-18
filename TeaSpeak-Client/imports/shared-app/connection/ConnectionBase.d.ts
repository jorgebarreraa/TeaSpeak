import { CommandHelper } from "../connection/CommandHelper";
import { HandshakeHandler } from "../connection/HandshakeHandler";
import { CommandResult } from "../connection/ServerConnectionDeclaration";
import { ServerAddress } from "../tree/Server";
import { ConnectionHandler, ConnectionState } from "../ConnectionHandler";
import { AbstractCommandHandlerBoss } from "../connection/AbstractCommandHandler";
import { Registry } from "../events";
import { AbstractVoiceConnection } from "../connection/VoiceConnection";
import { VideoConnection } from "tc-shared/connection/VideoConnection";
export interface CommandOptions {
    flagset?: string[];
    process_result?: boolean;
    timeout?: number;
}
export declare const CommandOptionDefaults: CommandOptions;
export declare type ConnectionPing = {
    javascript: number | undefined;
    native: number;
};
export interface ServerConnectionEvents {
    notify_connection_state_changed: {
        oldState: ConnectionState;
        newState: ConnectionState;
    };
    notify_ping_updated: {
        newPing: ConnectionPing;
    };
}
export declare type ConnectionStateListener = (old_state: ConnectionState, new_state: ConnectionState) => any;
export declare type ConnectionStatistics = {
    bytesReceived: number;
    bytesSend: number;
};
export declare abstract class AbstractServerConnection {
    readonly events: Registry<ServerConnectionEvents>;
    readonly client: ConnectionHandler;
    readonly command_helper: CommandHelper;
    protected connectionState: ConnectionState;
    protected constructor(client: ConnectionHandler);
    abstract connect(address: ServerAddress, handshake: HandshakeHandler, timeout?: number): Promise<void>;
    abstract connected(): boolean;
    abstract disconnect(reason?: string): Promise<void>;
    abstract getServerType(): "teaspeak" | "teamspeak" | "unknown";
    abstract getVoiceConnection(): AbstractVoiceConnection;
    abstract getVideoConnection(): VideoConnection;
    abstract getCommandHandler(): AbstractCommandHandlerBoss;
    abstract send_command(command: string, data?: any | any[], options?: CommandOptions): Promise<CommandResult>;
    abstract remote_address(): ServerAddress;
    connectionProxyAddress(): ServerAddress | undefined;
    abstract handshake_handler(): HandshakeHandler;
    abstract getControlStatistics(): ConnectionStatistics;
    updateConnectionState(state: ConnectionState): void;
    getConnectionState(): ConnectionState;
    abstract ping(): ConnectionPing;
}
export declare class ServerCommand {
    command: string;
    arguments: any[];
    switches: string[];
    constructor(command: string, payload: any[], switches: string[]);
    getString(key: string, index?: number): string;
    getInt(key: string, index?: number): number;
    getUInt(key: string, index?: number): number;
}
export interface SingleCommandHandler {
    name?: string;
    command?: string | string[];
    timeout?: number;
    function: (command: ServerCommand) => boolean;
}
