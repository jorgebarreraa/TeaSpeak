import { AbstractServerConnection } from "./connection/ConnectionBase";
import { PermissionManager } from "./permission/PermissionManager";
import { GroupManager } from "./permission/GroupManager";
import { SoundManager } from "./audio/Sounds";
import { RecorderProfile } from "./voice/RecorderProfile";
import { Registry } from "./events";
import { FileManager } from "./file/FileManager";
import { PluginCmdRegistry } from "./connection/PluginCmdHandler";
import { ServerFeatures } from "./connection/ServerFeatures";
import { ChannelTree } from "./tree/ChannelTree";
import { LocalClientEntry } from "./tree/Client";
import { ChannelVideoFrame } from "tc-shared/ui/frames/video/Controller";
import { ChannelConversationManager } from "./conversations/ChannelConversationManager";
import { PrivateConversationManager } from "tc-shared/conversations/PrivateConversationManager";
import { SelectedClientInfo } from "./SelectedClientInfo";
import { SideBarManager } from "tc-shared/SideBarManager";
import { ServerEventLog } from "tc-shared/connectionlog/ServerEventLog";
import { PlaylistManager } from "tc-shared/music/PlaylistManager";
import { ConnectParameters } from "tc-shared/ui/modal/connect/Controller";
import { ServerSettings } from "tc-shared/ServerSettings";
export declare enum InputHardwareState {
    MISSING = 0,
    START_FAILED = 1,
    VALID = 2
}
export declare enum DisconnectReason {
    HANDLER_DESTROYED = 0,
    REQUESTED = 1,
    DNS_FAILED = 2,
    CONNECT_FAILURE = 3,
    CONNECTION_CLOSED = 4,
    CONNECTION_FATAL_ERROR = 5,
    CONNECTION_PING_TIMEOUT = 6,
    CLIENT_KICKED = 7,
    CLIENT_BANNED = 8,
    HANDSHAKE_FAILED = 9,
    HANDSHAKE_TEAMSPEAK_REQUIRED = 10,
    HANDSHAKE_BANNED = 11,
    SERVER_CLOSED = 12,
    SERVER_REQUIRES_PASSWORD = 13,
    SERVER_HOSTMESSAGE = 14,
    IDENTITY_TOO_LOW = 15,
    UNKNOWN = 16
}
export declare enum ConnectionState {
    UNCONNECTED = 0,
    CONNECTING = 1,
    INITIALISING = 2,
    AUTHENTICATING = 3,
    CONNECTED = 4,
    DISCONNECTING = 5
}
export declare namespace ConnectionState {
    function socketConnected(state: ConnectionState): boolean;
    function fullyConnected(state: ConnectionState): boolean;
}
export declare enum ViewReasonId {
    VREASON_USER_ACTION = 0,
    VREASON_MOVED = 1,
    VREASON_SYSTEM = 2,
    VREASON_TIMEOUT = 3,
    VREASON_CHANNEL_KICK = 4,
    VREASON_SERVER_KICK = 5,
    VREASON_BAN = 6,
    VREASON_SERVER_STOPPED = 7,
    VREASON_SERVER_LEFT = 8,
    VREASON_CHANNEL_UPDATED = 9,
    VREASON_EDITED = 10,
    VREASON_SERVER_SHUTDOWN = 11
}
export interface LocalClientStatus {
    input_muted: boolean;
    output_muted: boolean;
    lastChannelCodecWarned: number;
    away: boolean | string;
    channel_subscribe_all: boolean;
    queries_visible: boolean;
}
export interface ConnectParametersOld {
    nickname?: string;
    channel?: {
        target: string | number;
        password?: string;
    };
    token?: string;
    password?: {
        password: string;
        hashed: boolean;
    };
    auto_reconnect_attempt?: boolean;
}
export declare class ConnectionHandler {
    readonly handlerId: string;
    private readonly events_;
    channelTree: ChannelTree;
    connection_state: ConnectionState;
    serverConnection: AbstractServerConnection;
    currentConnectId: number;
    fileManager: FileManager;
    permissions: PermissionManager;
    groups: GroupManager;
    video_frame: ChannelVideoFrame;
    settings: ServerSettings;
    sound: SoundManager;
    serverFeatures: ServerFeatures;
    log: ServerEventLog;
    private sideBar;
    private playlistManager;
    private channelConversations;
    private privateConversations;
    private clientInfoManager;
    private localClientId;
    private localClient;
    private autoReconnectTimer;
    private isReconnectAttempt;
    private connectAttemptId;
    private echoTestRunning;
    private pluginCmdRegistry;
    private handlerState;
    private clientStatusSync;
    private inputHardwareState;
    private listenerRecorderInputDeviceChanged;
    constructor();
    initializeHandlerState(source?: ConnectionHandler): void;
    events(): Registry<ConnectionEvents>;
    startConnectionNew(parameters: ConnectParameters, isReconnectAttempt: boolean): Promise<void>;
    disconnectFromServer(reason?: string): Promise<void>;
    getClient(): LocalClientEntry;
    getClientId(): number;
    getPrivateConversations(): PrivateConversationManager;
    getChannelConversations(): ChannelConversationManager;
    getSelectedClientInfo(): SelectedClientInfo;
    getSideBar(): SideBarManager;
    getPlaylistManager(): PlaylistManager;
    initializeLocalClient(clientId: number, acceptedName: string): void;
    getServerConnection(): AbstractServerConnection;
    private handleConnectionStateChanged;
    get connected(): boolean;
    private generate_ssl_certificate_accept;
    private _certificate_modal;
    handleDisconnect(type: DisconnectReason, data?: any): void;
    cancelAutoReconnect(log_event: boolean): void;
    private updateVoiceStatus;
    private lastRecordErrorPopup;
    update_voice_status(): void;
    sync_status_with_server(): void;
    acquireInputHardware(): Promise<void>;
    startVoiceRecorder(notifyError: boolean): Promise<{
        state: "success" | "no-input";
    } | {
        state: "error";
        message: string;
    }>;
    getVoiceRecorder(): RecorderProfile | undefined;
    generateReconnectParameters(): ConnectParameters | undefined;
    private initializeWhisperSession;
    destroy(): void;
    setMicrophoneMuted(muted: boolean, dontPlaySound?: boolean): void;
    toggleMicrophone(): void;
    isMicrophoneMuted(): boolean;
    isMicrophoneDisabled(): boolean;
    setSpeakerMuted(muted: boolean, dontPlaySound?: boolean): void;
    toggleSpeakerMuted(): void;
    isSpeakerMuted(): boolean;
    isSpeakerDisabled(): boolean;
    setSubscribeToAllChannels(flag: boolean): void;
    isSubscribeToAllChannels(): boolean;
    setAway(state: boolean | string): void;
    private doSetAway;
    toggleAway(): void;
    isAway(): boolean;
    setQueriesShown(flag: boolean): void;
    areQueriesShown(): boolean;
    getInputHardwareState(): InputHardwareState;
    private setInputHardwareState;
    hasOutputHardware(): boolean;
    getPluginCmdRegistry(): PluginCmdRegistry;
    startEchoTest(): Promise<void>;
    stopEchoTest(): void;
    getCurrentServerUniqueId(): string;
}
export declare type ConnectionStateUpdateType = "microphone" | "speaker" | "away" | "subscribe" | "query";
export interface ConnectionEvents {
    notify_state_updated: {
        state: ConnectionStateUpdateType;
    };
    notify_connection_state_changed: {
        oldState: ConnectionState;
        newState: ConnectionState;
    };
    notify_handler_initialized: {};
}
