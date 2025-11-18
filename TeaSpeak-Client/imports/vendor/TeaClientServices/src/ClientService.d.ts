import { ClientSessionType } from "./Messages";
import { ClientServiceConnection } from "./Connection";
import { Registry } from "tc-events";
export declare type LocalAgent = {
    clientVersion: string;
    uiVersion: string;
    architecture: string;
    platform: string;
    platformVersion: string;
};
export interface ClientServiceConfig {
    getServiceHost(): string;
    getSelectedLocaleUrl(): string | null;
    getSessionType(): ClientSessionType;
    generateHostInfo(): LocalAgent;
}
export interface ClientServiceEvents {
    /** Client service session has successfully be initialized */
    notify_session_initialized: {};
    /** The current active client service session has been closed */
    notify_session_closed: {};
}
export declare class ClientServices {
    readonly config: ClientServiceConfig;
    readonly events: Registry<ClientServiceEvents>;
    private readonly connection;
    private sessionInitialized;
    private retryTimer;
    private initializeAgentId;
    private initializeLocaleId;
    constructor(config: ClientServiceConfig);
    start(): void;
    awaitSession(): Promise<void>;
    isSessionInitialized(): boolean;
    stop(): void;
    getConnection(): ClientServiceConnection;
    private scheduleRetry;
    /**
     * Returns as soon the result indicates that something else went wrong rather than transmitting.
     * Note: This will not throw an exception!
     * @param command
     * @param retryInterval
     */
    private executeCommandWithRetry;
    /**
     * @returns `true` if the session agent has been successfully initialized.
     */
    private sendInitializeAgent;
    private sendLocaleUpdate;
}
