import { AbstractServerConnection } from "../connection/ConnectionBase";
import { ConnectParameters } from "tc-shared/ui/modal/connect/Controller";
export interface HandshakeIdentityHandler {
    connection: AbstractServerConnection;
    executeHandshake(): any;
    registerCallback(callback: (success: boolean, message?: string) => any): any;
    fillClientInitData(data: any): any;
}
export declare class HandshakeHandler {
    private connection;
    private handshakeImpl;
    private handshakeFailed;
    readonly parameters: ConnectParameters;
    constructor(parameters: ConnectParameters);
    setConnection(con: AbstractServerConnection): void;
    initialize(): void;
    get_identity_handler(): HandshakeIdentityHandler;
    startHandshake(): void;
    on_teamspeak(): void;
    private handshake_failed;
    private handleHandshakeFinished;
}
