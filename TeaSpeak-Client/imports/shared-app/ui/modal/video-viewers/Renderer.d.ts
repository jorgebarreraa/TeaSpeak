import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import React from "react";
import { IpcRegistryDescription } from "tc-events";
import { ModalVideoViewersEvents, ModalVideoViewersVariables } from "tc-shared/ui/modal/video-viewers/Definitions";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
declare class Modal extends AbstractModal {
    private readonly events;
    private readonly variables;
    constructor(events: IpcRegistryDescription<ModalVideoViewersEvents>, variables: IpcVariableDescriptor<ModalVideoViewersVariables>);
    protected onDestroy(): void;
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
}
export default Modal;
