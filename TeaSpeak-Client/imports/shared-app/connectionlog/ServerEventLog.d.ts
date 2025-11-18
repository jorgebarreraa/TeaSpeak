import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { Registry } from "tc-shared/events";
import { LogMessage, TypeInfo } from "tc-shared/connectionlog/Definitions";
export interface ServerEventLogEvents {
    notify_log_add: {
        event: LogMessage;
    };
}
export declare class ServerEventLog {
    readonly events: Registry<ServerEventLogEvents>;
    private readonly connection;
    private maxHistoryLength;
    private eventLog;
    constructor(connection: ConnectionHandler);
    log<T extends keyof TypeInfo>(type: T, data: TypeInfo[T]): void;
    getHistory(): LogMessage[];
    destroy(): void;
}
