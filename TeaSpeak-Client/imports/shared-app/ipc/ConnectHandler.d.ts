import { BasicIPCHandler } from "../ipc/BrowserIPC";
export declare type ConnectRequestData = {
    address: string;
    profile?: string;
    username?: string;
    password?: {
        value: string;
        hashed: boolean;
    };
};
export interface ConnectOffer {
    request_id: string;
    data: ConnectRequestData;
}
export interface ConnectOfferAnswer {
    request_id: string;
    accepted: boolean;
}
export interface ConnectExecute {
    request_id: string;
}
export interface ConnectExecuted {
    request_id: string;
    succeeded: boolean;
    message?: string;
}
export declare class ConnectHandler {
    private static readonly CHANNEL_NAME;
    readonly ipc_handler: BasicIPCHandler;
    private ipc_channel;
    callback_available: (data: ConnectRequestData) => boolean;
    callback_execute: (data: ConnectRequestData) => boolean | string;
    private _pending_connect_offers;
    private _pending_connects_requests;
    constructor(ipc_handler: BasicIPCHandler);
    setup(): void;
    private onMessage;
    post_connect_request(data: ConnectRequestData, callback_avail: () => Promise<boolean>): Promise<void>;
}
