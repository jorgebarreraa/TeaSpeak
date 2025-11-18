import { ConnectUiEvents, ConnectUiVariables } from "tc-shared/ui/modal/connect/Definitions";
import * as React from "react";
import { IpcRegistryDescription } from "tc-shared/events";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
import { AbstractModal } from "tc-shared/ui/react-elements/ModalDefinitions";
declare class ConnectModal extends AbstractModal {
    private readonly events;
    private readonly variables;
    private readonly connectNewTabByDefault;
    constructor(events: IpcRegistryDescription<ConnectUiEvents>, variables: IpcVariableDescriptor<ConnectUiVariables>, connectNewTabByDefault: boolean);
    protected onDestroy(): void;
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
    verticalAlignment(): "top" | "center" | "bottom";
}
export = ConnectModal;
