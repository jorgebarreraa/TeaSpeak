import { AbstractHandshakeIdentityHandler, HandshakeCommandHandler, IdentitifyType, Identity } from "../../profiles/Identity";
import { AbstractServerConnection } from "../../connection/ConnectionBase";
import { HandshakeIdentityHandler } from "../../connection/HandshakeHandler";
export declare namespace CryptoHelper {
    function base64UrlEncode(str: any): any;
    function base64UrlDecode(str: string, pad?: boolean): string;
    function arraybufferToString(buf: any): string;
    function export_ecc_key(crypto_key: CryptoKey, public_key: boolean): Promise<string>;
    function decryptTeaSpeakIdentity(buffer: Uint8Array): Promise<string>;
    function encryptTeaSpeakIdentity(buffer: Uint8Array): Promise<string>;
    /**
     * @param buffer base64 encoded ASN.1 string
     */
    function decodeTomCryptKey(buffer: string): {
        crv: string;
        d: any;
        x: any;
        y: any;
        ext: boolean;
        key_ops: string[];
        kty: string;
    };
}
export declare class TeaSpeakHandshakeHandler extends AbstractHandshakeIdentityHandler {
    identity: TeaSpeakIdentity;
    handler: HandshakeCommandHandler<TeaSpeakHandshakeHandler>;
    constructor(connection: AbstractServerConnection, identity: TeaSpeakIdentity);
    executeHandshake(): void;
    private handle_proof;
    protected trigger_fail(message: string): void;
    protected trigger_success(): void;
    fillClientInitData(data: any): void;
}
export declare class TeaSpeakIdentity implements Identity {
    static generateNew(): Promise<TeaSpeakIdentity>;
    static import_ts(ts_string: string, ini?: boolean): Promise<TeaSpeakIdentity>;
    hash_number: string;
    private_key: string;
    _name: string;
    publicKey: string;
    private _initialized;
    private _crypto_key;
    private _crypto_key_sign;
    private _unique_id;
    constructor(private_key?: string, hash?: string, name?: string, initialize?: boolean);
    fallback_name(): string | undefined;
    uid(): string;
    type(): IdentitifyType;
    valid(): boolean;
    decode(data: string): Promise<void>;
    encode?(): string;
    level(): Promise<number>;
    private static calculateLevel;
    /**
     * @param {string} a
     * @param {string} b
     * @description b must be smaller (in bytes) then a
     */
    private static string_add;
    improve_level_for(time: number, threads: number): Promise<Boolean>;
    improveLevelNative(target: number, threads: number, active_callback: () => boolean, callback_level?: (current: number) => any, callback_status?: (hash_rate: number) => any): Promise<Boolean>;
    improveLevelJavascript(target: number, activeCallback: () => boolean): Promise<number>;
    private initialize;
    export_ts(ini?: boolean): Promise<string>;
    sign_message(message: string, hash?: string): Promise<string>;
    spawn_identity_handshake_handler(connection: AbstractServerConnection): HandshakeIdentityHandler;
}
