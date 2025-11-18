import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import React from "react";
import { IpcRegistryDescription } from "tc-events";
import { ModalServerInfoEvents, ModalServerInfoVariables } from "tc-shared/ui/modal/server-info/Definitions";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
declare class Modal extends AbstractModal {
    private readonly events;
    private readonly variables;
    constructor(events: IpcRegistryDescription<ModalServerInfoEvents>, variables: IpcVariableDescriptor<ModalServerInfoVariables>);
    protected onDestroy(): void;
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
}
export default Modal;
