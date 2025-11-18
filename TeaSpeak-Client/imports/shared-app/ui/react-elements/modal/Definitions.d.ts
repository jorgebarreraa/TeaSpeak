import { IpcRegistryDescription, Registry } from "tc-shared/events";
import { ChannelEditEvents } from "tc-shared/ui/modal/channel-edit/Definitions";
import { EchoTestEvents } from "tc-shared/ui/modal/echo-test/Definitions";
import { ModalGlobalSettingsEditorEvents } from "tc-shared/ui/modal/global-settings-editor/Definitions";
import { InviteUiEvents, InviteUiVariables } from "tc-shared/ui/modal/invite/Definitions";
import React, { ReactElement } from "react";
import { IpcVariableDescriptor } from "tc-shared/ui/utils/IpcVariable";
import { ModalBookmarkEvents, ModalBookmarkVariables } from "tc-shared/ui/modal/bookmarks/Definitions";
import { ModalBookmarksAddServerEvents, ModalBookmarksAddServerVariables } from "tc-shared/ui/modal/bookmarks-add-server/Definitions";
import { ModalPokeEvents, ModalPokeVariables } from "tc-shared/ui/modal/poke/Definitions";
import { ModalClientGroupAssignmentEvents, ModalClientGroupAssignmentVariables } from "tc-shared/ui/modal/group-assignment/Definitions";
import { VideoViewerEvents } from "tc-shared/ui/modal/video-viewer/Definitions";
import { PermissionModalEvents } from "tc-shared/ui/modal/permission/ModalDefinitions";
import { PermissionEditorEvents } from "tc-shared/ui/modal/permission/EditorDefinitions";
import { PermissionEditorServerInfo } from "tc-shared/ui/modal/permission/ModalRenderer";
import { ModalAvatarUploadEvents, ModalAvatarUploadVariables } from "tc-shared/ui/modal/avatar-upload/Definitions";
import { ModalInputProcessorEvents, ModalInputProcessorVariables } from "tc-shared/ui/modal/input-processor/Definitios";
import { ModalServerInfoEvents, ModalServerInfoVariables } from "tc-shared/ui/modal/server-info/Definitions";
import { ModalAboutVariables } from "tc-shared/ui/modal/about/Definitions";
import { ModalServerBandwidthEvents } from "tc-shared/ui/modal/server-bandwidth/Definitions";
import { ModalYesNoEvents, ModalYesNoVariables } from "tc-shared/ui/modal/yes-no/Definitions";
import { ModalChannelInfoEvents, ModalChannelInfoVariables } from "tc-shared/ui/modal/channel-info/Definitions";
import { ModalVideoViewersEvents, ModalVideoViewersVariables } from "tc-shared/ui/modal/video-viewers/Definitions";
export declare type ModalType = "error" | "warning" | "info" | "none";
export declare type ModalRenderType = "page" | "dialog";
export interface ModalOptions {
    /**
     * Unique modal id.
     */
    uniqueId?: string;
    /**
     * Destroy the modal if it has been closed.
     * If the value is `false` it *might* destroy the modal anyways.
     * Default: `true`.
     */
    destroyOnClose?: boolean;
    /**
     * Default size of the modal in pixel.
     * This value might or might not be respected.
     */
    defaultSize?: {
        width: number;
        height: number;
    };
    /**
     * Determines if the modal is resizeable or now.
     * Some browsers might not support non resizeable modals.
     * Default: `both`
     */
    resizeable?: "none" | "vertical" | "horizontal" | "both";
    /**
     * If the modal should be popoutable.
     * Default: `false`
     */
    popoutable?: boolean;
    /**
     * The default popout state.
     * Default: `false`
     */
    popedOut?: boolean;
}
export interface ModalEvents {
    "open": {};
    "close": {};
    "destroy": {};
}
export declare enum ModalState {
    SHOWN = 0,
    HIDDEN = 1,
    DESTROYED = 2
}
export interface ModalInstanceEvents {
    action_close: {};
    action_minimize: {};
    action_popout: {};
    notify_open: {};
    notify_minimize: {};
    notify_close: {};
    notify_destroy: {};
}
export interface ModalInstanceController {
    getState(): ModalState;
    getEvents(): Registry<ModalInstanceEvents>;
    show(): Promise<void>;
    hide(): Promise<void>;
    minimize(): Promise<void>;
    maximize(): Promise<void>;
    destroy(): any;
}
export interface ModalController {
    getOptions(): Readonly<ModalOptions>;
    getEvents(): Registry<ModalEvents>;
    getState(): ModalState;
    show(): Promise<void>;
    hide(): Promise<void>;
    destroy(): any;
}
export interface ModalInstanceProperties {
    windowed: boolean;
}
export declare abstract class AbstractModal {
    protected readonly properties: ModalInstanceProperties;
    protected constructor();
    abstract renderBody(): ReactElement;
    abstract renderTitle(): string | React.ReactElement;
    type(): ModalType;
    color(): "none" | "blue" | "red";
    verticalAlignment(): "top" | "center" | "bottom";
    /** @deprecated */
    protected onInitialize(): void;
    protected onDestroy(): void;
    protected onClose(): void;
    protected onOpen(): void;
}
export declare abstract class InternalModal extends AbstractModal {
}
export declare function constructAbstractModalClass<T extends keyof ModalConstructorArguments>(klass: new (...args: ModalConstructorArguments[T]) => AbstractModal, properties: ModalInstanceProperties, args: ModalConstructorArguments[T]): AbstractModal;
export interface ModalConstructorArguments {
    "__internal__modal__": any[];
    "modal-yes-no": [
        IpcRegistryDescription<ModalYesNoEvents>,
        IpcVariableDescriptor<ModalYesNoVariables>
    ];
    "video-viewer": [
        IpcRegistryDescription<VideoViewerEvents>,
        string
    ];
    "channel-edit": [
        IpcRegistryDescription<ChannelEditEvents>,
        boolean
    ];
    "channel-info": [
        IpcRegistryDescription<ModalChannelInfoEvents>,
        IpcVariableDescriptor<ModalChannelInfoVariables>
    ];
    "echo-test": [
        IpcRegistryDescription<EchoTestEvents>
    ];
    "global-settings-editor": [
        IpcRegistryDescription<ModalGlobalSettingsEditorEvents>
    ];
    "conversation": any;
    "css-editor": any;
    "channel-tree": any;
    "modal-connect": any;
    "modal-invite": [
        IpcRegistryDescription<InviteUiEvents>,
        IpcVariableDescriptor<InviteUiVariables>,
        string
    ];
    "modal-bookmarks": [
        IpcRegistryDescription<ModalBookmarkEvents>,
        IpcVariableDescriptor<ModalBookmarkVariables>
    ];
    "modal-bookmark-add-server": [
        IpcRegistryDescription<ModalBookmarksAddServerEvents>,
        IpcVariableDescriptor<ModalBookmarksAddServerVariables>
    ];
    "modal-poked": [
        IpcRegistryDescription<ModalPokeEvents>,
        IpcVariableDescriptor<ModalPokeVariables>
    ];
    "modal-assign-server-groups": [
        IpcRegistryDescription<ModalClientGroupAssignmentEvents>,
        IpcVariableDescriptor<ModalClientGroupAssignmentVariables>
    ];
    "modal-permission-edit": [
        PermissionEditorServerInfo,
        IpcRegistryDescription<PermissionModalEvents>,
        IpcRegistryDescription<PermissionEditorEvents>
    ];
    "modal-avatar-upload": [
        IpcRegistryDescription<ModalAvatarUploadEvents>,
        IpcVariableDescriptor<ModalAvatarUploadVariables>,
        string
    ];
    "modal-input-processor": [
        IpcRegistryDescription<ModalInputProcessorEvents>,
        IpcVariableDescriptor<ModalInputProcessorVariables>
    ];
    "modal-about": [
        IpcRegistryDescription,
        IpcVariableDescriptor<ModalAboutVariables>
    ];
    "modal-server-info": [
        IpcRegistryDescription<ModalServerInfoEvents>,
        IpcVariableDescriptor<ModalServerInfoVariables>
    ];
    "modal-server-bandwidth": [
        IpcRegistryDescription<ModalServerBandwidthEvents>
    ];
    "modal-video-viewers": [
        IpcRegistryDescription<ModalVideoViewersEvents>,
        IpcVariableDescriptor<ModalVideoViewersVariables>
    ];
}
