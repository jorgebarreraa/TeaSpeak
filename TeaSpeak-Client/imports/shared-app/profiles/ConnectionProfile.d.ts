import { IdentitifyType, Identity } from "../profiles/Identity";
import { AbstractServerConnection } from "../connection/ConnectionBase";
import { HandshakeIdentityHandler } from "../connection/HandshakeHandler";
export declare class ConnectionProfile {
    id: string;
    profileName: string;
    defaultUsername: string;
    defaultPassword: string;
    selectedIdentityType: string;
    identities: {
        [key: string]: Identity;
    };
    constructor(id: string);
    connectUsername(): string;
    selectedIdentity(current_type?: IdentitifyType): Identity;
    selectedType(): IdentitifyType | undefined;
    setIdentity(type: IdentitifyType, identity: Identity): void;
    spawnIdentityHandshakeHandler(connection: AbstractServerConnection): HandshakeIdentityHandler | undefined;
    encode(): string;
    valid(): boolean;
}
export declare function createConnectProfile(name: string, id?: string): ConnectionProfile;
export declare function save(): void;
export declare function mark_need_save(): void;
export declare function requires_save(): boolean;
export declare function availableConnectProfiles(): ConnectionProfile[];
export declare function findConnectProfile(id: string): ConnectionProfile | undefined;
export declare function find_profile_by_name(name: string): ConnectionProfile | undefined;
export declare function defaultConnectProfile(): ConnectionProfile;
export declare function set_default_profile(profile: ConnectionProfile): ConnectionProfile;
export declare function delete_profile(profile: ConnectionProfile): void;
