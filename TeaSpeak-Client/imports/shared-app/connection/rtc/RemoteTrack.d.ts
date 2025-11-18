import { Registry } from "tc-shared/events";
export interface TrackClientInfo {
    media?: number;
    client_id: number;
    client_database_id: number;
    client_unique_id: string;
    client_name: string;
}
export declare enum RemoteRTPTrackState {
    /** The track isn't bound to any client */
    Unbound = 0,
    /** The track is bound to a client, but isn't replaying anything */
    Bound = 1,
    /** The track is currently replaying something (inherits the Bound characteristics) */
    Started = 2,
    /** The track has been destroyed */
    Destroyed = 3
}
export interface RemoteRTPTrackEvents {
    notify_state_changed: {
        oldState: RemoteRTPTrackState;
        newState: RemoteRTPTrackState;
    };
}
declare global {
    interface RTCRtpReceiver {
        playoutDelayHint: number;
    }
}
export declare class RemoteRTPTrack {
    protected readonly events: Registry<RemoteRTPTrackEvents>;
    private readonly ssrc;
    private readonly transceiver;
    private currentState;
    protected currentAssignment: TrackClientInfo;
    constructor(ssrc: number, transceiver: RTCRtpTransceiver);
    protected destroy(): void;
    getEvents(): Registry<RemoteRTPTrackEvents>;
    getState(): RemoteRTPTrackState;
    getSsrc(): number;
    getTrack(): MediaStreamTrack;
    getTransceiver(): RTCRtpTransceiver;
    getCurrentAssignment(): TrackClientInfo | undefined;
    protected setState(state: RemoteRTPTrackState): void;
}
export declare class RemoteRTPVideoTrack extends RemoteRTPTrack {
    protected mediaStream: MediaStream;
    constructor(ssrc: number, transceiver: RTCRtpTransceiver);
    getMediaStream(): MediaStream;
    protected handleTrackEnded(): void;
}
export declare class RemoteRTPAudioTrack extends RemoteRTPTrack {
    protected htmlAudioNode: HTMLAudioElement;
    protected mediaStream: MediaStream;
    protected audioNode: MediaStreamAudioSourceNode;
    protected gainNode: GainNode;
    protected shouldReplay: boolean;
    protected gain: number;
    constructor(ssrc: number, transceiver: RTCRtpTransceiver);
    protected handleTrackEnded(): void;
    getGain(): GainNode | undefined;
    setGain(value: number): void;
    /**
     * Mutes this track until the next setGain(..) call or a new sequence begins (state update)
     */
    abortCurrentReplay(): void;
    protected updateGainNode(): void;
}
