import { ConnectionHandler } from "tc-shared/ConnectionHandler";
export declare type ClientInfoResult = {
    status: "success";
    clientName: string;
    clientUniqueId: string;
    clientDatabaseId: number;
} | {
    status: "not-found";
} | {
    status: "error";
    error: string;
};
export declare class ClientInfoResolver {
    private readonly handler;
    private readonly requestDatabaseIds;
    private readonly requestUniqueIds;
    private executed;
    constructor(handler: ConnectionHandler);
    private registerRequest;
    private fullFullAllRequests;
    private static parseClientInfo;
    getInfoByDatabaseId(databaseId: number): Promise<ClientInfoResult>;
    getInfoByUniqueId(uniqueId: string): Promise<ClientInfoResult>;
    executeQueries(): Promise<void>;
}
