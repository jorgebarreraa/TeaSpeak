import { ConnectionHandler } from "../ConnectionHandler";
import { Registry } from "../events";
export declare type ServerFeatureSupport = "unsupported" | "supported" | "experimental" | "deprecated";
export declare enum ServerFeature {
    ERROR_BULKS = "error-bulks",
    ADVANCED_CHANNEL_CHAT = "advanced-channel-chat",
    LOG_QUERY = "log-query",
    WHISPER_ECHO = "whisper-echo",
    VIDEO = "video",
    SIDEBAR_MODE = "sidebar-mode"
}
export interface ServerFeatureEvents {
    notify_state_changed: {
        feature: ServerFeature;
        version?: number;
        support: ServerFeatureSupport;
    };
}
export declare class ServerFeatures {
    readonly events: Registry<ServerFeatureEvents>;
    private readonly connection;
    private readonly explicitCommandHandler;
    private readonly stateChangeListener;
    private featureAwait;
    private featureAwaitCallback;
    private featuresSet;
    private featureStates;
    constructor(connection: ConnectionHandler);
    destroy(): void;
    supportsFeature(feature: ServerFeature, version?: number): boolean;
    awaitFeatures(): Promise<boolean>;
    listenSupportChange(feature: ServerFeature, listener: (support: boolean) => void, version?: number): () => void;
    private disableAllFeatures;
    private setFeatureSupport;
}
