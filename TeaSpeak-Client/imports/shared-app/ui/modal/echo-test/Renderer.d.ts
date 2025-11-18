import * as React from "react";
import { IpcRegistryDescription } from "tc-shared/events";
import { EchoTestEvents } from "./Definitions";
import { Translatable } from "tc-shared/ui/react-elements/i18n";
import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
declare class ModalEchoTest extends AbstractModal {
    private readonly events;
    constructor(events: IpcRegistryDescription<EchoTestEvents>);
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement<Translatable>;
}
export = ModalEchoTest;
