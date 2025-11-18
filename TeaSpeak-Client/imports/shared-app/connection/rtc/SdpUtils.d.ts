export declare class SdpProcessor {
    private static readonly kAudioCodecs;
    private static readonly kVideoCodecs;
    private rtpRemoteChannelMapping;
    private rtpLocalChannelMapping;
    constructor();
    reset(): void;
    getRemoteSsrcFromFromMediaId(mediaId: string): number | undefined;
    getLocalSsrcFromFromMediaId(mediaId: string): number | undefined;
    getLocalMediaIdFromSsrc(ssrc: number): string | undefined;
    processIncomingSdp(sdpString: string, _mode: "offer" | "answer"): string;
    processOutgoingSdp(sdpString: string, _mode: "offer" | "answer"): string;
    private static generateRtpSSrcMapping;
    private static patchLocalCodecs;
}
export declare namespace SdpCompressor {
    function decompressSdp(sdp: string, mode: number): string;
    function compressSdp(sdp: string, mode: number): string;
}
