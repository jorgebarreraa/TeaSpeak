import { RecorderProfile } from "../voice/RecorderProfile";
import { AbstractServerConnection, ConnectionStatistics } from "../connection/ConnectionBase";
import { Registry } from "../events";
import { VoiceClient } from "../voice/VoiceClient";
import { WhisperSession, WhisperTarget } from "../voice/VoiceWhisper";
export declare enum VoiceConnectionStatus {
    ClientUnsupported = 0,
    ServerUnsupported = 1,
    Connecting = 2,
    Connected = 3,
    Disconnecting = 4,
    Disconnected = 5,
    Failed = 6
}
export interface VoiceConnectionEvents {
    "notify_connection_status_changed": {
        oldStatus: VoiceConnectionStatus;
        newStatus: VoiceConnectionStatus;
    };
    "notify_recorder_changed": {
        oldRecorder: RecorderProfile | undefined;
        newRecorder: RecorderProfile | undefined;
    };
    "notify_whisper_created": {
        session: WhisperSession;
    };
    "notify_whisper_initialized": {
        session: WhisperSession;
    };
    "notify_whisper_destroyed": {
        session: WhisperSession;
    };
    "notify_voice_replay_state_change": {
        replaying: boolean;
    };
}
export declare type WhisperSessionInitializeData = {
    clientName: string;
    clientUniqueId: string;
    sessionTimeout: number;
    blocked: boolean;
    volume: number;
};
export declare type WhisperSessionInitializer = (session: WhisperSession) => Promise<WhisperSessionInitializeData>;
export declare abstract class AbstractVoiceConnection {
    readonly events: Registry<VoiceConnectionEvents>;
    readonly connection: AbstractServerConnection;
    protected constructor(connection: AbstractServerConnection);
    abstract getConnectionState(): VoiceConnectionStatus;
    abstract getFailedMessage(): string;
    abstract getRetryTimestamp(): number | 0;
    abstract getConnectionStats(): Promise<ConnectionStatistics>;
    abstract encodingSupported(codec: number): boolean;
    abstract decodingSupported(codec: number): boolean;
    abstract registerVoiceClient(clientId: number): VoiceClient;
    abstract availableVoiceClients(): VoiceClient[];
    abstract unregisterVoiceClient(client: VoiceClient): any;
    abstract voiceRecorder(): RecorderProfile;
    abstract acquireVoiceRecorder(recorder: RecorderProfile | undefined): Promise<void>;
    abstract getEncoderCodec(): number;
    abstract setEncoderCodec(codec: number): any;
    abstract stopAllVoiceReplays(): any;
    abstract isReplayingVoice(): boolean;
    abstract getWhisperSessions(): WhisperSession[];
    abstract dropWhisperSession(session: WhisperSession): any;
    abstract setWhisperSessionInitializer(initializer: WhisperSessionInitializer | undefined): any;
    abstract getWhisperSessionInitializer(): WhisperSessionInitializer | undefined;
    abstract startWhisper(target: WhisperTarget): Promise<void>;
    abstract getWhisperTarget(): WhisperTarget | undefined;
    abstract stopWhisper(): any;
}
