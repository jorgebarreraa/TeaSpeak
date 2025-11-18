import { AbstractServerConnection, ServerCommand } from "../connection/ConnectionBase";
import { HandshakeIdentityHandler } from "../connection/HandshakeHandler";
import { AbstractCommandHandler } from "../connection/AbstractCommandHandler";
export declare enum IdentitifyType {
    TEAFORO = 0,
    TEAMSPEAK = 1,
    NICKNAME = 2
}
export interface Identity {
    fallback_name(): string | undefined;
    uid(): string;
    type(): IdentitifyType;
    valid(): boolean;
    encode?(): string;
    decode(data: string): Promise<void>;
    spawn_identity_handshake_handler(connection: AbstractServerConnection): HandshakeIdentityHandler;
}
export declare function decode_identity(type: IdentitifyType, data: string): Promise<Identity>;
export declare function create_identity(type: IdentitifyType): Identity;
export declare class HandshakeCommandHandler<T extends AbstractHandshakeIdentityHandler> extends AbstractCommandHandler {
    readonly handle: T;
    constructor(connection: AbstractServerConnection, handle: T);
    handle_command(command: ServerCommand): boolean;
}
export declare abstract class AbstractHandshakeIdentityHandler implements HandshakeIdentityHandler {
    connection: AbstractServerConnection;
    protected callbacks: ((success: boolean, message?: string) => any)[];
    protected constructor(connection: AbstractServerConnection);
    registerCallback(callback: (success: boolean, message?: string) => any): void;
    fillClientInitData(data: any): void;
    abstract executeHandshake(): any;
    protected trigger_success(): void;
    protected trigger_fail(message: string): void;
}
