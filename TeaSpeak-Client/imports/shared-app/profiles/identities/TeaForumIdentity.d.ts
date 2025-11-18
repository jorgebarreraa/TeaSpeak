import { IdentitifyType, Identity } from "../../profiles/Identity";
import { AbstractServerConnection } from "../../connection/ConnectionBase";
import { HandshakeIdentityHandler } from "../../connection/HandshakeHandler";
import * as forum from "./teaspeak-forum";
export declare class TeaForumIdentity implements Identity {
    private readonly identity_data;
    valid(): boolean;
    constructor(data: forum.Data);
    data(): forum.Data;
    decode(data: any): Promise<void>;
    encode(): string;
    spawn_identity_handshake_handler(connection: AbstractServerConnection): HandshakeIdentityHandler;
    fallback_name(): string | undefined;
    type(): IdentitifyType;
    uid(): string;
    static identity(): TeaForumIdentity;
}
export declare function set_static_identity(identity: TeaForumIdentity): void;
export declare function update_forum(): void;
export declare function valid_static_forum_identity(): boolean;
export declare function static_forum_identity(): TeaForumIdentity | undefined;
