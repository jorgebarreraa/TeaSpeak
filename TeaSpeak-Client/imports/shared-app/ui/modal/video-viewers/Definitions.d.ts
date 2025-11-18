import { ClientIcon } from "svg-sprites/client-icons";
import { VideoBroadcastType } from "tc-shared/connection/VideoConnection";
export declare type VideoViewerInfo = {
    handlerId: string;
    clientName: string;
    clientUniqueId: string;
    clientStatus: ClientIcon | undefined;
};
export declare type VideoViewerList = {
    [T in VideoBroadcastType]?: number[];
} & {
    __internal_client_order: number[];
};
export interface ModalVideoViewersVariables {
    viewerInfo: VideoViewerInfo | undefined;
    videoViewers: VideoViewerList;
}
export interface ModalVideoViewersEvents {
}
