import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import React from "react";
import { IpcRegistryDescription } from "tc-events";
import { ModalClientGroupAssignmentEvents, ModalClientGroupAssignmentVariables } from "tc-shared/ui/modal/group-assignment/Definitions";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
export default class ModalServerGroups extends AbstractModal {
    private readonly events;
    private readonly variables;
    constructor(events: IpcRegistryDescription<ModalClientGroupAssignmentEvents>, variables: IpcVariableDescriptor<ModalClientGroupAssignmentVariables>);
    protected onDestroy(): void;
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
}
