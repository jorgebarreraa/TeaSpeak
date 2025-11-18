import { AbstractServerConnection } from "tc-shared/connection/ConnectionBase";
import { Registry } from "tc-shared/events";
import { RemoteRTPAudioTrack, RemoteRTPVideoTrack, TrackClientInfo } from "./RemoteTrack";
import { WhisperTarget } from "tc-shared/voice/VoiceWhisper";
import { VideoBroadcastConfig, VideoBroadcastType } from "tc-shared/connection/VideoConnection";
declare global {
    interface RTCIceCandidate {
        address: string | undefined;
    }
    interface HTMLCanvasElement {
        captureStream(framed: number): MediaStream;
    }
}
export declare type RtcVideoBroadcastStatistics = {
    dimensions: {
        width: number;
        height: number;
    };
    frameRate: number;
    codec?: {
        name: string;
        payloadType: number;
    };
    bandwidth?: {
        currentBps: number;
        maxBps: number;
    };
    qualityLimitation: "cpu" | "bandwidth" | "none";
    source: {
        frameRate: number;
        dimensions: {
            width: number;
            height: number;
        };
    };
};
export declare enum RTPConnectionState {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    FAILED = 3,
    NOT_SUPPORTED = 4
}
export declare type RTCSourceTrackType = "audio" | "audio-whisper" | "video" | "video-screen";
export declare type RTCBroadcastableTrackType = Exclude<RTCSourceTrackType, "audio-whisper">;
export declare type RTCConnectionStatistics = {
    videoBytesReceived: number;
    videoBytesSent: number;
    voiceBytesReceived: number;
    voiceBytesSent: any;
};
export interface RTCConnectionEvents {
    notify_state_changed: {
        oldState: RTPConnectionState;
        newState: RTPConnectionState;
    };
    notify_audio_assignment_changed: {
        track: RemoteRTPAudioTrack;
        info: TrackClientInfo | undefined;
    };
    notify_video_assignment_changed: {
        track: RemoteRTPVideoTrack;
        info: TrackClientInfo | undefined;
    };
}
export declare class RTCConnection {
    static readonly kEnableSdpTrace = true;
    private readonly audioSupport;
    private readonly events;
    private readonly connection;
    private readonly commandHandler;
    private readonly sdpProcessor;
    private connectionState;
    private connectTimeout;
    private failedReason;
    private retryCalculator;
    private retryTimestamp;
    private retryTimeout;
    private peer;
    private localCandidateCount;
    private peerRemoteDescriptionReceived;
    private cachedRemoteIceCandidates;
    private cachedRemoteSessionDescription;
    private currentTracks;
    private currentTransceiver;
    private remoteAudioTracks;
    private remoteVideoTracks;
    private temporaryStreams;
    constructor(connection: AbstractServerConnection, audioSupport: boolean);
    destroy(): void;
    isAudioEnabled(): boolean;
    getConnection(): AbstractServerConnection;
    getEvents(): Registry<RTCConnectionEvents>;
    getConnectionState(): RTPConnectionState;
    getFailReason(): string;
    getRetryTimestamp(): number | 0;
    restartConnection(): void;
    reset(updateConnectionState: boolean): void;
    setTrackSource(type: RTCSourceTrackType, source: MediaStreamTrack | null): Promise<MediaStreamTrack>;
    clearTrackSources(types: RTCSourceTrackType[]): Promise<MediaStreamTrack[]>;
    getTrackTypeFromSsrc(ssrc: number): RTCSourceTrackType | undefined;
    startVideoBroadcast(type: VideoBroadcastType, config: VideoBroadcastConfig): Promise<void>;
    changeVideoBroadcastConfig(type: VideoBroadcastType, config: VideoBroadcastConfig): Promise<void>;
    startAudioBroadcast(): Promise<void>;
    startWhisper(target: WhisperTarget): Promise<void>;
    stopTrackBroadcast(type: RTCBroadcastableTrackType): void;
    setNotSupported(): void;
    private updateConnectionState;
    private handleFatalError;
    private static checkBrowserSupport;
    doInitialSetup(): void;
    private updateTracks;
    private doInitialSetup0;
    private handleConnectionStateChanged;
    private handleLocalIceCandidate;
    handleRemoteIceCandidate(candidate: RTCIceCandidate | undefined, mediaLine: number): void;
    applyCachedRemoteIceCandidates(): void;
    private handleIceCandidateError;
    private handleIceConnectionStateChanged;
    private handleIceGatheringStateChanged;
    private handleSignallingStateChanged;
    private handleNegotiationNeeded;
    private handlePeerConnectionStateChanged;
    private handleDataChannel;
    private releaseTemporaryStream;
    private handleTrack;
    private getOrCreateTempStream;
    private doMapStream;
    private handleStreamState;
    getConnectionStatistics(): Promise<RTCConnectionStatistics>;
    getVideoBroadcastStatistics(type: RTCBroadcastableTrackType): Promise<RtcVideoBroadcastStatistics | undefined>;
}
