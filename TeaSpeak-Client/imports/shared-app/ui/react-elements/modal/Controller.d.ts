import { InternalModal, ModalConstructorArguments, ModalController, ModalEvents, ModalOptions, ModalState } from "tc-shared/ui/react-elements/modal/Definitions";
import { Registry } from "tc-events";
export declare class GenericModalController<T extends keyof ModalConstructorArguments> implements ModalController {
    private readonly events;
    private readonly modalType;
    private readonly modalConstructorArguments;
    private readonly modalOptions;
    private modalKlass;
    private instance;
    private popedOut;
    static fromInternalModal<ModalClass extends InternalModal>(klass: new (...args: any[]) => ModalClass, constructorArguments: any[]): GenericModalController<"__internal__modal__">;
    constructor(modalType: T, constructorArguments: ModalConstructorArguments[T], options: ModalOptions);
    private getModalClass;
    private createModalInstance;
    private destroyModalInstance;
    destroy(): void;
    getEvents(): Registry<ModalEvents>;
    getOptions(): Readonly<ModalOptions>;
    getState(): ModalState;
    hide(): Promise<void>;
    show(): Promise<void>;
}
