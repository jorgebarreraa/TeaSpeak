import { AbstractModal, ModalInstanceController, ModalInstanceEvents, ModalOptions, ModalState } from "tc-shared/ui/react-elements/modal/Definitions";
import * as React from "react";
import { RegisteredModal } from "tc-shared/ui/react-elements/modal/Registry";
import { Registry } from "tc-events";
declare class InternalRendererInstance extends React.PureComponent<{
    instance: InternalModalInstance;
}, {
    shown: boolean;
}> {
    constructor(props: any);
    render(): JSX.Element;
    componentWillUnmount(): void;
}
export declare class InternalModalInstance implements ModalInstanceController {
    readonly instanceUniqueId: string;
    readonly events: Registry<ModalInstanceEvents>;
    readonly refRendererInstance: React.RefObject<InternalRendererInstance>;
    private readonly modalKlass;
    private readonly constructorArguments;
    private readonly modalOptions;
    private state;
    modalInstance: AbstractModal;
    private modalInitializePromise;
    constructor(modalType: RegisteredModal<any>, constructorArguments: any[], modalOptions: ModalOptions);
    private constructModal;
    private destructModal;
    private destructModalInstance;
    getState(): ModalState;
    getEvents(): Registry<ModalInstanceEvents>;
    show(): Promise<void>;
    hide(): Promise<void>;
    minimize(): Promise<void>;
    maximize(): Promise<void>;
    destroy(): void;
    getCloseCallback(): () => void;
    getPopoutCallback(): () => void;
    getMinimizeCallback(): any;
}
export declare const InternalModalHook: React.MemoExoticComponent<() => JSX.Element>;
export {};
