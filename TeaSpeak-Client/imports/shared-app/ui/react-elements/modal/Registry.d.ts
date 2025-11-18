import { AbstractModal } from "../../../ui/react-elements/ModalDefinitions";
import { ModalConstructorArguments } from "tc-shared/ui/react-elements/modal/Definitions";
export interface RegisteredModal<T extends keyof ModalConstructorArguments> {
    modalId: T;
    classLoader: () => Promise<{
        default: new (...args: ModalConstructorArguments[T]) => AbstractModal;
    }>;
    popoutSupported: boolean;
}
export declare function findRegisteredModal<T extends keyof ModalConstructorArguments>(name: T): RegisteredModal<T> | undefined;
