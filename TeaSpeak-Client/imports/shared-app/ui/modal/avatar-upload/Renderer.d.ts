import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import React from "react";
import { IpcRegistryDescription } from "tc-events";
import { ModalAvatarUploadEvents, ModalAvatarUploadVariables } from "tc-shared/ui/modal/avatar-upload/Definitions";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
declare class ModalAvatarUpload extends AbstractModal {
    private readonly serverUniqueId;
    private readonly events;
    private readonly variables;
    constructor(events: IpcRegistryDescription<ModalAvatarUploadEvents>, variables: IpcVariableDescriptor<ModalAvatarUploadVariables>, serverUniqueId: string);
    protected onDestroy(): void;
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
}
export default ModalAvatarUpload;
