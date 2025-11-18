export declare type ModalInstanceInitializeResult = {
    status: "success";
    modalId: string;
    modalType: string;
    constructorArguments: any[];
} | {
    status: "timeout";
} | {
    status: "rejected";
    message: string;
};
export declare class ModalWindowControllerInstance {
    private readonly ipcMessageHandler;
    private ipcRemotePeerId;
    private ipcChannel;
    constructor(modalChannelId: string);
    initialize(): Promise<ModalInstanceInitializeResult>;
    triggerModalAction(modalId: string, action: "minimize" | "close"): void;
    private registerIpcMessageHandler;
    private sendIpcMessage;
}
