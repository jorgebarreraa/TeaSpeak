import { AbstractModal } from "tc-shared/ui/react-elements/ModalDefinitions";
import "./ModalRenderer.scss";
export interface ModalControlFunctions {
    close(): any;
    minimize(): any;
}
export declare class ModalRenderer {
    private readonly functionController;
    private readonly container;
    constructor(functionController: ModalControlFunctions);
    renderModal(modal: AbstractModal | undefined): void;
}
