export declare type ConnectionStatus = {
    type: "healthy";
    bytesReceived?: number;
    bytesSend?: number;
} | {
    type: "unhealthy";
    reason: string;
    retryTimestamp: number;
} | {
    type: "connecting-signalling";
    state: "initializing" | "connecting" | "authentication";
} | {
    type: "connecting-voice";
} | {
    type: "connecting-video";
} | {
    type: "disconnected";
} | {
    type: "unsupported";
    side: "server" | "client";
};
export declare type ConnectionComponent = "signaling" | "video" | "voice";
export interface ConnectionStatusEvents {
    action_toggle_component_detail: {
        shown: boolean | undefined;
    };
    query_component_detail_state: {};
    query_component_status: {
        component: ConnectionComponent;
    };
    query_connection_status: {};
    notify_component_detail_state: {
        shown: boolean;
    };
    notify_component_status: {
        component: ConnectionComponent;
        status: ConnectionStatus;
    };
    notify_connection_status: {
        status: ConnectionStatus;
    };
}
