import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { HostBannerUiEvents } from "tc-shared/ui/frames/HostBannerDefinitions";
import { Registry } from "tc-shared/events";
export declare class HostBannerController {
    readonly uiEvents: Registry<HostBannerUiEvents>;
    private currentConnection;
    private listenerConnection;
    constructor();
    destroy(): void;
    setConnectionHandler(handler: ConnectionHandler): void;
    protected initializeConnectionHandler(handler: ConnectionHandler): void;
    private notifyHostBanner;
}
