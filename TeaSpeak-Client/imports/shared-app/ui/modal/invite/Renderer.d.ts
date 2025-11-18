import * as React from "react";
import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import { IpcRegistryDescription } from "tc-events";
import { InviteUiEvents, InviteUiVariables } from "tc-shared/ui/modal/invite/Definitions";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
declare class ModalInvite extends AbstractModal {
    private readonly events;
    private readonly variables;
    private readonly serverName;
    constructor(events: IpcRegistryDescription<InviteUiEvents>, variables: IpcVariableDescriptor<InviteUiVariables>, serverName: string);
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
}
export = ModalInvite;
