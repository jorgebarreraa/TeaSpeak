import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
import { ModalBookmarkEvents, ModalBookmarkVariables } from "tc-shared/ui/modal/bookmarks/Definitions";
import { IpcRegistryDescription, Registry } from "tc-events";
import { UiVariableConsumer } from "tc-shared/ui/utils/Variable";
import * as React from "react";
declare class ModalBookmarks extends AbstractModal {
    readonly events: Registry<ModalBookmarkEvents>;
    readonly variables: UiVariableConsumer<ModalBookmarkVariables>;
    constructor(events: IpcRegistryDescription<ModalBookmarkEvents>, variables: IpcVariableDescriptor<ModalBookmarkVariables>);
    protected onDestroy(): void;
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
}
export = ModalBookmarks;
