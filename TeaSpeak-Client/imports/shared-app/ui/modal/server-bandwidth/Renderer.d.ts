import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import React from "react";
import { IpcRegistryDescription } from "tc-events";
import { ModalServerBandwidthEvents } from "tc-shared/ui/modal/server-bandwidth/Definitions";
declare class Modal extends AbstractModal {
    private readonly events;
    constructor(events: IpcRegistryDescription<ModalServerBandwidthEvents>);
    protected onDestroy(): void;
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
}
export default Modal;
