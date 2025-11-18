import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import React from "react";
import { IpcRegistryDescription } from "tc-events";
import { ModalAboutEvents, ModalAboutVariables } from "tc-shared/ui/modal/about/Definitions";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
declare class Modal extends AbstractModal {
    private readonly events;
    private readonly variables;
    constructor(events: IpcRegistryDescription<ModalAboutEvents>, variables: IpcVariableDescriptor<ModalAboutVariables>);
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
}
export default Modal;
