export declare const kUnknownHistoryServerUniqueId = "unknown";
export declare type ConnectionHistoryEntry = {
    id: number;
    timestamp: number;
    serverUniqueId: string | typeof kUnknownHistoryServerUniqueId;
    targetAddress: string;
    nickname: string;
    hashedPassword: string;
};
export declare type ConnectionHistoryServerEntry = {
    firstConnectTimestamp: number;
    firstConnectId: number;
    lastConnectTimestamp: number;
    lastConnectId: number;
};
export declare type ConnectionHistoryServerInfo = {
    name: string;
    iconId: number;
    country: string;
    clientsOnline: number | -1;
    clientsMax: number | -1;
    hostBannerUrl: string | undefined;
    hostBannerMode: number;
    passwordProtected: boolean;
};
export declare class ConnectionHistory {
    private database;
    constructor();
    initializeDatabase(): Promise<void>;
    /**
     * Register a new connection attempt.
     * @param attempt
     * @return Returns a unique connect attempt identifier id which could be later used to set the unique server id.
     */
    logConnectionAttempt(attempt: {
        targetAddress: string;
        nickname: string;
        hashedPassword: string;
    }): Promise<number>;
    private resolveDatabaseServerInfo;
    private updateDatabaseServerInfo;
    /**
     * Update the connection attempts target server id.
     * @param connectionAttemptId
     * @param serverUniqueId
     */
    updateConnectionServerUniqueId(connectionAttemptId: number, serverUniqueId: string): Promise<void>;
    /**
     * Update the connection attempt server password
     * @param connectionAttemptId
     * @param passwordHash
     */
    updateConnectionServerPassword(connectionAttemptId: number, passwordHash: string): Promise<void>;
    /**
     * Update the server info of the given server.
     * @param serverUniqueId
     * @param info
     */
    updateServerInfo(serverUniqueId: string, info: ConnectionHistoryServerInfo): Promise<void>;
    deleteConnectionAttempts(target: string, targetType: "address" | "server-unique-id"): Promise<void>;
    /**
     * Query the server info of a given server unique id
     * @param serverUniqueId
     */
    queryServerInfo(serverUniqueId: string): Promise<(ConnectionHistoryServerInfo & ConnectionHistoryServerEntry) | undefined>;
    /**
     * Query the last connected addresses/servers.
     * @param maxUniqueServers
     */
    lastConnectedServers(maxUniqueServers: number): Promise<ConnectionHistoryEntry[]>;
    lastConnectInfo(target: string, targetType: "address" | "server-unique-id", onlySucceeded?: boolean): Promise<ConnectionHistoryEntry | undefined>;
    countConnectCount(target: string, targetType: "address" | "server-unique-id"): Promise<number>;
}
export declare let connectionHistory: ConnectionHistory;
