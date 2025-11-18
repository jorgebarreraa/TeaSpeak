import { Registry } from "tc-shared/events";
import { ServerEventLogUiEvents } from "tc-shared/ui/frames/log/Definitions";
import { ConnectionHandler } from "tc-shared/ConnectionHandler";
export declare class ServerEventLogController {
    readonly events: Registry<ServerEventLogUiEvents>;
    private currentConnection;
    private listenerConnection;
    constructor();
    destroy(): void;
    setConnectionHandler(handler: ConnectionHandler): void;
    private sendLogs;
}
