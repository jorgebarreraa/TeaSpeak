import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { ConnectionComponent, ConnectionStatusEvents } from "./StatusDefinitions";
import { Registry } from "tc-shared/events";
export declare class StatusController {
    private readonly events;
    private currentConnectionHandler;
    private listenerHandler;
    private detailedInfoOpen;
    private detailUpdateTimer;
    private componentStatusNotifyState;
    private connectionStatusNotifyState;
    constructor(events: Registry<ConnectionStatusEvents>);
    getEvents(): Registry<ConnectionStatusEvents>;
    setConnectionHandler(handler: ConnectionHandler | undefined): void;
    setDetailsShown(flag: boolean): void;
    private registerHandlerEvents;
    private unregisterHandlerEvents;
    private getComponentStatus;
    notifyComponentStatus(component: ConnectionComponent): Promise<void>;
    private doNotifyComponentStatus;
    notifyConnectionStatus(): Promise<void>;
    private doNotifyConnectionStatus;
}
