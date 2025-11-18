import { Registry } from "tc-shared/events";
import * as React from "react";
import { ModalVideoSourceEvents } from "tc-shared/ui/modal/video-source/Definitions";
import { Translatable } from "tc-shared/ui/react-elements/i18n";
import { VideoBroadcastType } from "tc-shared/connection/VideoConnection";
import { InternalModal } from "tc-shared/ui/react-elements/modal/Definitions";
export declare class ModalVideoSource extends InternalModal {
    protected readonly events: Registry<ModalVideoSourceEvents>;
    private readonly sourceType;
    private readonly editMode;
    constructor(events: Registry<ModalVideoSourceEvents>, type: VideoBroadcastType, editMode: boolean);
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement<Translatable>;
}
