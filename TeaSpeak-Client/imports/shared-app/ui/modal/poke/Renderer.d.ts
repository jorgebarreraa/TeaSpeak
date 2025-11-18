import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import React from "react";
import { UiVariableConsumer } from "tc-shared/ui/utils/Variable";
import { ModalPokeEvents, ModalPokeVariables } from "tc-shared/ui/modal/poke/Definitions";
import { IpcRegistryDescription, Registry } from "tc-events";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
declare class PokeModal extends AbstractModal {
    readonly variables: UiVariableConsumer<ModalPokeVariables>;
    readonly events: Registry<ModalPokeEvents>;
    constructor(events: IpcRegistryDescription<ModalPokeEvents>, variables: IpcVariableDescriptor<ModalPokeVariables>);
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
    verticalAlignment(): "top" | "center" | "bottom";
}
export default PokeModal;
