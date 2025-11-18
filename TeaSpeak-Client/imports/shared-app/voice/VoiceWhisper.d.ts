import { Registry } from "../events";
import { VoicePlayer } from "../voice/VoicePlayer";
export interface WhisperTargetChannelClients {
    target: "channel-clients";
    channels: number[];
    clients: number[];
}
export interface WhisperTargetGroups {
    target: "groups";
}
export interface WhisperTargetEcho {
    target: "echo";
}
export declare type WhisperTarget = WhisperTargetGroups | WhisperTargetChannelClients | WhisperTargetEcho;
export interface WhisperSessionEvents {
    notify_state_changed: {
        oldState: WhisperSessionState;
        newState: WhisperSessionState;
    };
    notify_blocked_state_changed: {
        oldState: boolean;
        newState: boolean;
    };
    notify_timed_out: {};
}
export declare enum WhisperSessionState {
    INITIALIZING = 0,
    PAUSED = 1,
    PLAYING = 2,
    INITIALIZE_FAILED = 3
}
export declare const kUnknownWhisperClientUniqueId = "unknown";
export interface WhisperSession {
    readonly events: Registry<WhisperSessionEvents>;
    getClientId(): number;
    getClientName(): string | undefined;
    getClientUniqueId(): string | undefined;
    getSessionState(): WhisperSessionState;
    isBlocked(): boolean;
    setBlocked(blocked: boolean): any;
    getSessionTimeout(): number;
    setSessionTimeout(timeout: number): any;
    getLastWhisperTimestamp(): number;
    /**
     * This is only valid if the session has been initialized successfully,
     * and it hasn't been blocked
     *
     * @returns Returns the voice player
     */
    getVoicePlayer(): VoicePlayer | undefined;
}
