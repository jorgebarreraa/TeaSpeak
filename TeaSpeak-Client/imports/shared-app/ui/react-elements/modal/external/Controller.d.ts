import { Registry } from "tc-events";
import { ModalInstanceController, ModalInstanceEvents, ModalOptions, ModalState } from "tc-shared/ui/react-elements/modal/Definitions";
export declare class ExternalModalController implements ModalInstanceController {
    private readonly modalType;
    private readonly modalOptions;
    private readonly constructorArguments;
    private readonly mainModalId;
    private readonly ipcMessageHandler;
    private ipcRemotePeerId;
    private ipcChannel;
    private readonly modalEvents;
    private modalInitialized;
    private modalInitializeCallback;
    private windowId;
    private windowListener;
    private windowMutatePromise;
    constructor(modalType: string, constructorArguments: any[], modalOptions: ModalOptions);
    destroy(): void;
    getEvents(): Registry<ModalInstanceEvents>;
    getState(): ModalState;
    show(): Promise<void>;
    hide(): Promise<void>;
    minimize(): Promise<void>;
    maximize(): Promise<void>;
    private mutateWindow;
    private handleWindowDestroyed;
    private registerIpcMessageHandler;
    private sendIpcMessage;
}
