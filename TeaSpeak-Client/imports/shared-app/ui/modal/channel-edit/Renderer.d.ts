import * as React from "react";
import { IpcRegistryDescription } from "tc-shared/events";
import { ChannelEditEvents } from "tc-shared/ui/modal/channel-edit/Definitions";
import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
declare class ChannelEditModal extends AbstractModal {
    private readonly events;
    private readonly isChannelCreate;
    constructor(events: IpcRegistryDescription<ChannelEditEvents>, isChannelCreate: boolean);
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement;
    color(): "none" | "blue";
}
export = ChannelEditModal;
