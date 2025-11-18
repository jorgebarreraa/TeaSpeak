import { IdentitifyType, Identity } from "../../profiles/Identity";
import { AbstractServerConnection } from "../../connection/ConnectionBase";
import { HandshakeIdentityHandler } from "../../connection/HandshakeHandler";
export declare class NameIdentity implements Identity {
    private _name;
    constructor(name?: string);
    set_name(name: string): void;
    name(): string;
    fallback_name(): string | undefined;
    uid(): string;
    type(): IdentitifyType;
    valid(): boolean;
    decode(data: any): Promise<void>;
    encode?(): string;
    spawn_identity_handshake_handler(connection: AbstractServerConnection): HandshakeIdentityHandler;
}
