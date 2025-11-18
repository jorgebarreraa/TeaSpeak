import { AbstractServerConnection, ServerCommand, SingleCommandHandler } from "../connection/ConnectionBase";
export declare abstract class AbstractCommandHandler {
    readonly connection: AbstractServerConnection;
    handler_boss: AbstractCommandHandlerBoss | undefined;
    volatile_handler_boss: boolean;
    ignore_consumed: boolean;
    protected constructor(connection: AbstractServerConnection);
    /**
     * @return If the command should be consumed
     */
    abstract handle_command(command: ServerCommand): boolean;
}
export declare type CommandHandlerCallback = (command: ServerCommand, consumed: boolean) => void | boolean;
export declare abstract class AbstractCommandHandlerBoss {
    readonly connection: AbstractServerConnection;
    protected command_handlers: AbstractCommandHandler[];
    protected single_command_handler: SingleCommandHandler[];
    protected explicitHandlers: {
        [key: string]: CommandHandlerCallback[];
    };
    protected constructor(connection: AbstractServerConnection);
    destroy(): void;
    registerCommandHandler(command: string, callback: CommandHandlerCallback): () => void;
    unregisterCommandHandler(command: string, callback: CommandHandlerCallback): boolean;
    registerHandler(handler: AbstractCommandHandler): void;
    unregisterHandler(handler: AbstractCommandHandler): void;
    registerSingleHandler(handler: SingleCommandHandler): void;
    removeSingleHandler(handler: SingleCommandHandler): void;
    handlers(): AbstractCommandHandler[];
    invokeCommand(command: ServerCommand): boolean;
}
