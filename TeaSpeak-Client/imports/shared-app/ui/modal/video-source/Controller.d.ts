import { VideoSource } from "tc-shared/video/VideoSource";
import { VideoBroadcastConfig, VideoBroadcastType } from "tc-shared/connection/VideoConnection";
export declare type VideoSourceModalAction = {
    mode: "select-quick";
    defaultDevice?: string;
} | {
    mode: "select-default";
    defaultDevice?: string;
} | {
    mode: "new";
} | {
    mode: "edit";
    source: VideoSource;
    broadcastConstraints: VideoBroadcastConfig;
};
export declare type VideoSourceSelectResult = {
    source: VideoSource | undefined;
    config: VideoBroadcastConfig | undefined;
};
/**
 * @param type The video type which should be prompted
 * @param mode
 */
export declare function spawnVideoSourceSelectModal(type: VideoBroadcastType, mode: VideoSourceModalAction): Promise<VideoSourceSelectResult>;
