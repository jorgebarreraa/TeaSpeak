import { ReactComponentBase } from "tc-shared/ui/react-elements/ReactComponentBase";
import { Registry } from "tc-shared/events";
import * as React from "react";
import { ChannelTreeUIEvents } from "tc-shared/ui/tree/Definitions";
import { RDPChannelTree, RDPEntry } from "./RendererDataProvider";
export declare class PopoutButton extends React.Component<{
    tree: RDPChannelTree;
}, {}> {
    render(): JSX.Element;
}
export interface ChannelTreeViewProperties {
    events: Registry<ChannelTreeUIEvents>;
    dataProvider: RDPChannelTree;
    moveThreshold?: number;
}
export interface ChannelTreeViewState {
    elementScrollOffset?: number;
    scrollOffset: number;
    viewHeight: number;
    fontSize: number;
    treeVersion: number;
    smoothScroll: boolean;
    tree: RDPEntry[];
    treeRevision: number;
}
export declare class ChannelTreeView extends ReactComponentBase<ChannelTreeViewProperties, ChannelTreeViewState> {
    static readonly EntryHeightEm = 1.3;
    private readonly refContainer;
    private resizeObserver;
    private scrollFixRequested;
    private inViewCallbacks;
    constructor(props: any);
    componentDidMount(): void;
    componentWillUnmount(): void;
    private visibleEntries;
    render(): JSX.Element;
    private onScroll;
    scrollEntryInView(entryId: number, callback?: () => void): void;
    getEntryFromPoint(pageX: number, pageY: number): number;
}
