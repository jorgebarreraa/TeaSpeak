import { ClientIcon } from "svg-sprites/client-icons";
import { VideoBroadcastType } from "tc-shared/connection/VideoConnection";
export declare const kLocalVideoId = "__local__video__";
export declare const kLocalBroadcastChannels: VideoBroadcastType[];
export declare type ChannelVideoInfo = {
    clientName: string;
    clientUniqueId: string;
    clientId: number;
    statusIcon: ClientIcon;
};
export declare type ChannelVideoStreamState = "available" | "streaming" | "ignored" | "none";
export declare type VideoStatistics = {
    type: "sender";
    mode: "camara" | "screen";
    dimensions: {
        width: number;
        height: number;
    };
    frameRate: number;
    codec: {
        name: string;
        payloadType: number;
    };
    maxBandwidth: number;
    bandwidth: number;
    qualityLimitation: "cpu" | "bandwidth";
    source: {
        frameRate: number;
        dimensions: {
            width: number;
            height: number;
        };
    };
} | {
    type: "receiver";
    mode: "camara" | "screen";
    dimensions: {
        width: number;
        height: number;
    };
    frameRate: number;
    codec: {
        name: string;
        payloadType: number;
    };
};
export declare type VideoStreamState = {
    state: "disconnected";
} | {
    state: "available";
} | {
    state: "connecting";
} | {
    state: "failed";
    reason?: string;
} | {
    state: "connected";
    stream: MediaStream;
};
export declare type VideoSubscribeInfo = {
    totalSubscriptions: number;
    subscribedStreams: {
        [T in VideoBroadcastType]: number;
    };
    subscribeLimits: {
        [T in VideoBroadcastType]?: number;
    };
    maxSubscriptions: number | undefined;
};
/**
 * "muted": The video has been muted locally
 * "unset": The video will be normally played
 * "empty": No video available
 */
export declare type LocalVideoState = "muted" | "unset" | "empty";
export interface ChannelVideoEvents {
    action_toggle_expended: {
        expended: boolean;
    };
    action_video_scroll: {
        direction: "left" | "right";
    };
    action_toggle_spotlight: {
        videoIds: string[];
        enabled: boolean;
        expend: boolean;
    };
    action_focus_spotlight: {};
    action_set_fullscreen: {
        videoId: string | undefined;
    };
    action_set_pip: {
        videoId: string | undefined;
        broadcastType: VideoBroadcastType;
    };
    action_toggle_mute: {
        videoId: string;
        broadcastType: VideoBroadcastType | undefined;
        muted: boolean;
    };
    action_dismiss: {
        videoId: string;
        broadcastType: VideoBroadcastType;
    };
    action_show_viewers: {};
    query_expended: {};
    query_videos: {};
    query_video: {
        videoId: string;
    };
    query_video_info: {
        videoId: string;
    };
    query_video_statistics: {
        videoId: string;
        broadcastType: VideoBroadcastType;
    };
    query_spotlight: {};
    query_video_stream: {
        videoId: string;
        broadcastType: VideoBroadcastType;
    };
    query_subscribe_info: {};
    query_viewer_count: {};
    notify_expended: {
        expended: boolean;
    };
    notify_videos: {
        videoIds: string[];
    };
    notify_video: {
        videoId: string;
        cameraStream: ChannelVideoStreamState;
        screenStream: ChannelVideoStreamState;
    };
    notify_video_info: {
        videoId: string;
        info: ChannelVideoInfo;
    };
    notify_video_info_status: {
        videoId: string;
        statusIcon: ClientIcon;
    };
    notify_spotlight: {
        videoId: string[];
    };
    notify_video_statistics: {
        videoId: string | undefined;
        broadcastType: VideoBroadcastType;
        statistics: VideoStatistics;
    };
    notify_video_stream: {
        videoId: string;
        broadcastType: VideoBroadcastType;
        state: VideoStreamState;
    };
    notify_subscribe_info: {
        info: VideoSubscribeInfo;
    };
    notify_viewer_count: {
        camera: number | undefined;
        screen: number | undefined;
    };
}
export declare function makeVideoAutoplay(video: HTMLVideoElement): () => void;
