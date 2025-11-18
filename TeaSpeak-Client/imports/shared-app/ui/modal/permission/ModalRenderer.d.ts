import * as React from "react";
import { IpcRegistryDescription, Registry } from "tc-shared/events";
import { Translatable } from "tc-shared/ui/react-elements/i18n";
import { PermissionEditorEvents } from "tc-shared/ui/modal/permission/EditorDefinitions";
import { PermissionEditorTab, PermissionModalEvents } from "tc-shared/ui/modal/permission/ModalDefinitions";
import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
export declare type PermissionEditorServerInfo = {
    handlerId: string;
    serverUniqueId: string;
};
export declare const PermissionTabName: {
    [T in PermissionEditorTab]: {
        name: string;
        useTranslate: () => string;
        renderTranslate: () => React.ReactNode;
    };
};
export declare type DefaultTabValues = {
    groupId?: number;
    channelId?: number;
    clientDatabaseId?: number;
};
export declare class PermissionEditorModal extends AbstractModal {
    readonly serverInfo: PermissionEditorServerInfo;
    readonly modalEvents: Registry<PermissionModalEvents>;
    readonly editorEvents: Registry<PermissionEditorEvents>;
    constructor(serverInfo: PermissionEditorServerInfo, modalEvents: IpcRegistryDescription<PermissionModalEvents>, editorEvents: IpcRegistryDescription<PermissionEditorEvents>);
    protected onDestroy(): void;
    renderBody(): JSX.Element;
    renderTitle(): React.ReactElement<Translatable>;
}
export default PermissionEditorModal;
