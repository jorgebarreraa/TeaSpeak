import { Registry } from "tc-shared/events";
import { ChannelEntryInfo, ChannelIcons, ChannelTreeUIEvents, ClientIcons, ClientNameInfo, ClientTalkIconState, ServerState } from "tc-shared/ui/tree/Definitions";
import { ChannelTreeView, PopoutButton } from "tc-shared/ui/tree/RendererView";
import * as React from "react";
import { ChannelIconClass, ChannelIconsRenderer, RendererChannel } from "tc-shared/ui/tree/RendererChannel";
import { ClientIcon } from "svg-sprites/client-icons";
import { UnreadMarkerRenderer } from "tc-shared/ui/tree/RendererTreeEntry";
import { ClientIconsRenderer, ClientName, ClientStatus, ClientTalkStatusIcon, RendererClient } from "tc-shared/ui/tree/RendererClient";
import { ServerRenderer } from "tc-shared/ui/tree/RendererServer";
/**
 * auto      := Select/unselect/add/remove depending on the selected state & shift key state
 * exclusive := Only selected these entries
 * append    := Append these entries to the current selection
 * remove    := Remove these entries from the current selection
 */
export declare type RDPTreeSelectType = "auto" | "auto-add" | "exclusive" | "append" | "remove";
export declare class RDPTreeSelection {
    readonly handle: RDPChannelTree;
    selectedEntries: RDPEntry[];
    private readonly documentKeyListener;
    private readonly documentBlurListener;
    private shiftKeyPressed;
    private controlKeyPressed;
    private rangeStartEntry;
    private rangeMode;
    constructor(handle: RDPChannelTree);
    reset(): void;
    destroy(): void;
    isMultiSelect(): boolean;
    isAnythingSelected(): boolean;
    clearSelection(): void;
    select(entries: RDPEntry[], mode: RDPTreeSelectType, selectMainTree: boolean): void;
    selectNext(selectClients: boolean, direction: "up" | "down"): void;
    private doSelectNext;
}
export declare class RDPChannelTree {
    readonly events: Registry<ChannelTreeUIEvents>;
    readonly handlerId: string;
    private registeredEventHandlers;
    readonly refTree: React.RefObject<ChannelTreeView>;
    readonly refPopoutButton: React.RefObject<PopoutButton>;
    readonly selection: RDPTreeSelection;
    popoutShown: boolean;
    popoutButtonShown: boolean;
    private readonly documentDragStopListener;
    private treeRevision;
    private orderedTree;
    private treeEntries;
    private dragOverChannelEntry;
    constructor(events: Registry<ChannelTreeUIEvents>, handlerId: string);
    initialize(): void;
    destroy(): void;
    getTreeEntries(): RDPEntry[];
    handleDragStart(event: DragEvent): void;
    handleUiDragOver(event: DragEvent, target: RDPEntry): void;
    handleUiDrop(event: DragEvent, target: RDPEntry): void;
    private handleNotifyTreeEntries;
    private handleNotifyPopoutState;
}
export declare abstract class RDPEntry {
    readonly handle: RDPChannelTree;
    readonly entryId: number;
    readonly refUnread: React.RefObject<UnreadMarkerRenderer>;
    stateQueried: boolean;
    offsetTop: number;
    offsetLeft: number;
    selected: boolean;
    unread: boolean;
    private renderedInstance;
    protected constructor(handle: RDPChannelTree, entryId: number);
    destroy(): void;
    getEvents(): Registry<ChannelTreeUIEvents>;
    getHandlerId(): string;
    queryState(): void;
    handleUnreadUpdate(value: boolean): void;
    setSelected(value: boolean): void;
    handlePositionUpdate(offsetTop: number, offsetLeft: number): void;
    render(): React.ReactElement;
    select(mode: RDPTreeSelectType): void;
    handleUiDoubleClicked(): void;
    handleUiContextMenu(pageX: number, pageY: number): void;
    handleUiDragStart(event: DragEvent): void;
    handleUiDragOver(event: DragEvent): void;
    handleUiDrop(event: DragEvent): void;
    protected abstract doRender(): React.ReactElement;
    protected abstract renderSelectStateUpdate(): any;
    protected abstract renderPositionUpdate(): any;
}
export declare type RDPChannelDragHint = "none" | "top" | "bottom" | "contain";
export declare class RDPChannel extends RDPEntry {
    readonly refIcon: React.RefObject<ChannelIconClass>;
    readonly refIcons: React.RefObject<ChannelIconsRenderer>;
    readonly refChannel: React.RefObject<RendererChannel>;
    readonly refChannelContainer: React.RefObject<HTMLDivElement>;
    info: ChannelEntryInfo;
    icon: ClientIcon;
    icons: ChannelIcons;
    dragHint: "none" | "top" | "bottom" | "contain";
    constructor(handle: RDPChannelTree, entryId: number);
    doRender(): React.ReactElement;
    queryState(): void;
    renderSelectStateUpdate(): void;
    protected renderPositionUpdate(): void;
    handleIconUpdate(newIcon: ClientIcon): void;
    handleIconsUpdate(newIcons: ChannelIcons): void;
    handleInfoUpdate(newInfo: ChannelEntryInfo): void;
    setDragHint(hint: RDPChannelDragHint): void;
}
export declare class RDPClient extends RDPEntry {
    readonly refClient: React.RefObject<RendererClient>;
    readonly refStatus: React.RefObject<ClientStatus>;
    readonly refName: React.RefObject<ClientName>;
    readonly refTalkStatus: React.RefObject<ClientTalkStatusIcon>;
    readonly refIcons: React.RefObject<ClientIconsRenderer>;
    readonly localClient: boolean;
    name: ClientNameInfo;
    status: ClientIcon;
    icons: ClientIcons;
    rename: boolean;
    renameDefault: string;
    talkStatus: ClientTalkIconState;
    talkRequestMessage: string;
    constructor(handle: RDPChannelTree, entryId: number, localClient: boolean);
    doRender(): React.ReactElement;
    queryState(): void;
    protected renderPositionUpdate(): void;
    protected renderSelectStateUpdate(): void;
    handleStatusUpdate(newStatus: ClientIcon): void;
    handleNameUpdate(newName: ClientNameInfo): void;
    handleTalkStatusUpdate(newStatus: ClientTalkIconState, requestMessage: string): void;
    handleIconsUpdate(newIcons: ClientIcons): void;
    handleOpenRename(initialValue: string): void;
}
export declare class RDPServer extends RDPEntry {
    readonly refServer: React.RefObject<ServerRenderer>;
    state: ServerState;
    constructor(handle: RDPChannelTree, entryId: number);
    queryState(): void;
    protected doRender(): React.ReactElement;
    protected renderPositionUpdate(): void;
    protected renderSelectStateUpdate(): void;
    handleStateUpdate(newState: ServerState): void;
}
