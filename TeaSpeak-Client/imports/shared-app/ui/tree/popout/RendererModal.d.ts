import { AbstractModal } from "tc-shared/ui/react-elements/ModalDefinitions";
import { Registry } from "tc-shared/events";
import { ChannelTreeUIEvents } from "tc-shared/ui/tree/Definitions";
import * as React from "react";
import { ControlBarEvents } from "tc-shared/ui/frames/control-bar/Definitions";
import { ChannelTreePopoutConstructorArguments, ChannelTreePopoutEvents } from "tc-shared/ui/tree/popout/Definitions";
declare class ChannelTreeModal extends AbstractModal {
    readonly eventsUI: Registry<ChannelTreePopoutEvents>;
    readonly eventsTree: Registry<ChannelTreeUIEvents>;
    readonly eventsControlBar: Registry<ControlBarEvents>;
    readonly handlerId: string;
    constructor(info: ChannelTreePopoutConstructorArguments);
    protected onDestroy(): void;
    renderBody(): React.ReactElement;
    renderTitle(): React.ReactElement;
}
export = ChannelTreeModal;
