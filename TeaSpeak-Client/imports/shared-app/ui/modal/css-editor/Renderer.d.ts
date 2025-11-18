/// <reference types="react" />
import { CssEditorEvents } from "tc-shared/ui/modal/css-editor/Definitions";
import { IpcRegistryDescription } from "tc-shared/events";
import { AbstractModal } from "tc-shared/ui/react-elements/ModalDefinitions";
declare class PopoutConversationUI extends AbstractModal {
    private readonly events;
    constructor(events: IpcRegistryDescription<CssEditorEvents>);
    renderBody(): JSX.Element;
    renderTitle(): JSX.Element;
}
export = PopoutConversationUI;
