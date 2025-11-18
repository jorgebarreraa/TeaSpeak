import { LocalVideoBroadcast, VideoBroadcastType, VideoClient, VideoConnection, VideoConnectionEvent, VideoConnectionStatus } from "tc-shared/connection/VideoConnection";
import { Registry } from "tc-shared/events";
import { RTCConnection } from "../Connection";
import { RtpVideoClient } from "./VideoClient";
import { ConnectionStatistics } from "tc-shared/connection/ConnectionBase";
export declare class RtpVideoConnection implements VideoConnection {
    private readonly rtcConnection;
    private readonly events;
    private readonly listener;
    private connectionState;
    private broadcasts;
    private registeredClients;
    constructor(rtcConnection: RTCConnection);
    private setConnectionState;
    destroy(): void;
    getRTCConnection(): RTCConnection;
    getEvents(): Registry<VideoConnectionEvent>;
    getStatus(): VideoConnectionStatus;
    getRetryTimestamp(): number | 0;
    getFailedMessage(): string;
    registerVideoClient(clientId: number): RtpVideoClient;
    registeredVideoClients(): VideoClient[];
    unregisterVideoClient(client: VideoClient): void;
    private handleRtcConnectionStateChanged;
    private handleVideoAssignmentChanged;
    getConnectionStats(): Promise<ConnectionStatistics>;
    getLocalBroadcast(channel: VideoBroadcastType): LocalVideoBroadcast;
}
