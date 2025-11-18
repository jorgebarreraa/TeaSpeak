export interface ClientServiceLogger {
    logTrace(message: string, ...args: any[]): any;
    logDebug(message: string, ...args: any[]): any;
    logInfo(message: string, ...args: any[]): any;
    logWarn(message: string, ...args: any[]): any;
    logError(message: string, ...args: any[]): any;
    logCritical(message: string, ...args: any[]): any;
}
export declare let clientServiceLogger: ClientServiceLogger;
export declare function setClientServiceLogger(logger: ClientServiceLogger): void;
