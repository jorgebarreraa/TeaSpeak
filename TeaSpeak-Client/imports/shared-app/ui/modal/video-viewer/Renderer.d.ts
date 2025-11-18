import { Translatable } from "tc-shared/ui/react-elements/i18n";
import * as React from "react";
import { IpcRegistryDescription, Registry } from "tc-shared/events";
import { VideoViewerEvents } from "./Definitions";
import { AbstractModal } from "tc-shared/ui/react-elements/ModalDefinitions";
declare class ModalVideoPopout extends AbstractModal {
    readonly events: Registry<VideoViewerEvents>;
    readonly handlerId: string;
    constructor(events: IpcRegistryDescription<VideoViewerEvents>, handlerId: any);
    renderTitle(): string | React.ReactElement<Translatable>;
    renderBody(): React.ReactElement;
}
export = ModalVideoPopout;
