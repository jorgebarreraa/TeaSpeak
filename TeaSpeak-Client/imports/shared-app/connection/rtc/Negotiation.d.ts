export interface RTCNegotiationMediaMapping {
    direction: "sendrecv" | "recvonly" | "sendonly" | "inactive";
    ssrc: number;
}
export interface RTCNegotiationIceConfig {
    username: string;
    password: string;
    fingerprint: string;
    fingerprint_type: string;
    setup: "active" | "passive" | "actpass";
    candidates: string[];
}
export interface RTCNegotiationExtension {
    id: number;
    uri: string;
    media?: "audio" | "video";
    direction?: "recvonly" | "sendonly";
    config?: string;
}
export interface RTCNegotiationCodec {
    payload: number;
    name: string;
    channels?: number;
    rate?: number;
    fmtp?: string;
    feedback?: string[];
}
/** The offer send by the client to the server */
export interface RTCNegotiationOffer {
    type: "initial-offer" | "negotiation-offer";
    sessionId: number;
    ssrcs: number[];
    ssrc_types: number[];
    ice: RTCNegotiationIceConfig;
    extension: RTCNegotiationExtension | undefined;
}
/** The offer send by the server to the client */
export interface RTCNegotiationMessage {
    type: "initial-offer" | "negotiation-offer" | "initial-answer" | "negotiation-answer";
    sessionId: number;
    sessionUsername: string;
    ssrc: number[];
    ssrc_flags: number[];
    ice: RTCNegotiationIceConfig;
    extension: RTCNegotiationExtension[] | undefined;
    audio_codecs: RTCNegotiationCodec[] | undefined;
    video_codecs: RTCNegotiationCodec[] | undefined;
}
export declare class RTCNegotiator {
    private readonly peer;
    callbackData: (data: string) => void;
    callbackFailed: (reason: string) => void;
    private sessionCodecs;
    private sessionExtensions;
    constructor(peer: RTCPeerConnection);
    doInitialNegotiation(): void;
    handleRemoteData(dataString: string): void;
}
