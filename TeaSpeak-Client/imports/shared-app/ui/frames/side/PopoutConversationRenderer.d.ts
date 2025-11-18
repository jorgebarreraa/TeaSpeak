/// <reference types="react" />
import { IpcRegistryDescription } from "tc-shared/events";
import { AbstractConversationUiEvents } from "./AbstractConversationDefinitions";
import { AbstractModal } from "tc-shared/ui/react-elements/ModalDefinitions";
declare class PopoutConversationRenderer extends AbstractModal {
    private readonly events;
    private readonly userData;
    constructor(events: IpcRegistryDescription<AbstractConversationUiEvents>, userData: any);
    renderBody(): JSX.Element;
    renderTitle(): string;
}
export = PopoutConversationRenderer;
