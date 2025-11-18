import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import React from "react";
import { IpcRegistryDescription } from "tc-events";
import { ModalBookmarksAddServerEvents, ModalBookmarksAddServerVariables } from "tc-shared/ui/modal/bookmarks-add-server/Definitions";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
declare class ModalBookmarksAddServer extends AbstractModal {
    private readonly variables;
    private readonly events;
    constructor(events: IpcRegistryDescription<ModalBookmarksAddServerEvents>, variables: IpcVariableDescriptor<ModalBookmarksAddServerVariables>);
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
}
export = ModalBookmarksAddServer;
