import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import React from "react";
import { IpcRegistryDescription } from "tc-events";
import { ModalInputProcessorEvents, ModalInputProcessorVariables } from "tc-shared/ui/modal/input-processor/Definitios";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
declare class Modal extends AbstractModal {
    private readonly events;
    private readonly variables;
    constructor(events: IpcRegistryDescription<ModalInputProcessorEvents>, variables: IpcVariableDescriptor<ModalInputProcessorVariables>);
    protected onDestroy(): void;
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
}
export default Modal;
