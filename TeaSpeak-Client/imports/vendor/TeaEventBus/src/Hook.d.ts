export interface EventRegistryHooks {
    logTrace(message: string, ...args: any[]): any;
    logAsyncInvokeError(error: any): any;
    logReactInvokeError(error: any): any;
}
export declare let eventRegistryHooks: EventRegistryHooks;
export declare function setEventRegistryHooks(hooks: EventRegistryHooks): void;
