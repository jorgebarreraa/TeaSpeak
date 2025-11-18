import { ConnectionProfile } from "tc-shared/profiles/ConnectionProfile";
export declare type ConnectParameters = {
    targetAddress: string;
    serverPassword?: string;
    serverPasswordHashed?: boolean;
    nickname: string;
    nicknameSpecified: boolean;
    profile: ConnectionProfile;
    token?: string;
    defaultChannel?: string | number;
    defaultChannelPassword?: string;
    defaultChannelPasswordHashed?: boolean;
};
export declare type ConnectModalOptions = {
    connectInANewTab?: boolean;
    selectedAddress?: string;
    selectedProfile?: ConnectionProfile;
};
export declare function spawnConnectModalNew(options: ConnectModalOptions): void;
