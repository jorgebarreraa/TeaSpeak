export interface ModalIPCMessages {
    "hello-renderer": {
        version: string;
    };
    "hello-controller": {
        accepted: true;
        modalId: string;
        modalType: string;
        constructorArguments: any[];
    } | {
        accepted: false;
        message: string;
    };
    "invoke-modal-action": {
        modalId: string;
        action: "close" | "minimize";
    };
    "invalidate-modal-instance": {};
}
export declare type ModalIPCRenderer2ControllerMessages = Pick<ModalIPCMessages, "hello-renderer" | "invoke-modal-action">;
export declare type ModalIPCController2Renderer = Pick<ModalIPCMessages, "hello-controller" | "invalidate-modal-instance">;
export declare type ModalIPCMessage<Messages, T extends keyof Messages = keyof Messages> = {
    type: T;
    payload: Messages[T];
};
