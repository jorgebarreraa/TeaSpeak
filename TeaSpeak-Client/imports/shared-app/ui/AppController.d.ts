import { ConnectionHandler } from "tc-shared/ConnectionHandler";
export declare class AppController {
    private uiEvents;
    private listener;
    private currentConnection;
    private listenerConnection;
    private container;
    private controlBarEvents;
    private connectionListEvents;
    private sideBarController;
    private serverLogController;
    private hostBannerController;
    constructor();
    destroy(): void;
    initialize(): void;
    setConnectionHandler(connection: ConnectionHandler): void;
    renderApp(): void;
    private notifyChannelTree;
    private notifyVideoContainer;
}
export declare let appViewController: AppController;
