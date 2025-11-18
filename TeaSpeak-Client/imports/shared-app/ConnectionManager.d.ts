import { ConnectionHandler } from "./ConnectionHandler";
import { Registry } from "./events";
export interface ConnectionManagerEvents {
    notify_handler_created: {
        handlerId: string;
        handler: ConnectionHandler;
    };
    notify_active_handler_changed: {
        oldHandler: ConnectionHandler | undefined;
        newHandler: ConnectionHandler | undefined;
        oldHandlerId: string | undefined;
        newHandlerId: string | undefined;
    };
    notify_handler_deleted: {
        handlerId: string;
        handler: ConnectionHandler;
    };
    notify_handler_order_changed: {};
}
export declare class ConnectionManager {
    private readonly events_;
    private connectionHandlers;
    private activeConnectionHandler;
    constructor();
    events(): Registry<ConnectionManagerEvents>;
    spawnConnectionHandler(): ConnectionHandler;
    destroyConnectionHandler(handler: ConnectionHandler): void;
    setActiveConnectionHandler(handler: ConnectionHandler): void;
    private doSetActiveConnectionHandler;
    swapHandlerOrder(handlerA: ConnectionHandler, handlerB: ConnectionHandler): void;
    findConnection(handlerId: string): ConnectionHandler | undefined;
    getActiveConnectionHandler(): ConnectionHandler | undefined;
    getAllConnectionHandlers(): ConnectionHandler[];
}
export declare let server_connections: ConnectionManager;
