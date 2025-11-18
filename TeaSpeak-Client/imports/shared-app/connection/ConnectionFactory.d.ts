import { AbstractServerConnection } from "../connection/ConnectionBase";
import { ConnectionHandler } from "../ConnectionHandler";
export interface ServerConnectionFactory {
    create(client: ConnectionHandler): AbstractServerConnection;
    destroy(instance: AbstractServerConnection): any;
}
export declare function setServerConnectionFactory(factory: ServerConnectionFactory): void;
export declare function getServerConnectionFactory(): ServerConnectionFactory;
