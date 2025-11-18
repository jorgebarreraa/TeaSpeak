import { ConnectionHandler } from "../ConnectionHandler";
import { CommandResult } from "../connection/ServerConnectionDeclaration";
import { AbstractServerConnection } from "../connection/ConnectionBase";
export interface PluginCommandInvoker {
    clientId: number;
    clientUniqueId: string;
    clientName: string;
}
export declare abstract class PluginCmdHandler {
    protected readonly channel: string;
    protected currentServerConnection: AbstractServerConnection;
    protected constructor(channel: string);
    handleHandlerUnregistered(): void;
    handleHandlerRegistered(): void;
    getChannel(): string;
    abstract handlePluginCommand(data: string, invoker: PluginCommandInvoker): any;
    protected sendPluginCommand(data: string, mode: "server" | "view" | "channel" | "private", clientOrChannelId?: number): Promise<CommandResult>;
}
export declare class PluginCmdRegistry {
    readonly connection: ConnectionHandler;
    private readonly handler;
    private handlerMap;
    constructor(connection: ConnectionHandler);
    destroy(): void;
    registerHandler(handler: PluginCmdHandler): void;
    unregisterHandler(handler: PluginCmdHandler): void;
    private handlePluginCommand;
    getPluginHandler<T extends PluginCmdHandler>(channel: string): T | undefined;
}
