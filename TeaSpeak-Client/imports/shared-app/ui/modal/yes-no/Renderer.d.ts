import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import { IpcRegistryDescription } from "tc-events";
import { ModalYesNoEvents, ModalYesNoVariables } from "tc-shared/ui/modal/yes-no/Definitions";
import React from "react";
declare class Modal extends AbstractModal {
    private readonly events;
    private readonly variables;
    constructor(events: IpcRegistryDescription<ModalYesNoEvents>, variables: IpcRegistryDescription<ModalYesNoVariables>);
    protected onDestroy(): void;
    color(): "none" | "blue" | "red";
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
}
export default Modal;
