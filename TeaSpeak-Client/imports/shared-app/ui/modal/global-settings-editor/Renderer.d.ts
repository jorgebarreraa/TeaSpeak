import { Translatable } from "tc-shared/ui/react-elements/i18n";
import * as React from "react";
import { IpcRegistryDescription, Registry } from "tc-shared/events";
import { ModalGlobalSettingsEditorEvents } from "tc-shared/ui/modal/global-settings-editor/Definitions";
import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
declare class ModalGlobalSettingsEditor extends AbstractModal {
    protected readonly events: Registry<ModalGlobalSettingsEditorEvents>;
    constructor(events: IpcRegistryDescription<ModalGlobalSettingsEditorEvents>);
    renderBody(): React.ReactElement;
    renderTitle(): string | React.ReactElement<Translatable>;
}
export = ModalGlobalSettingsEditor;
