import { Registry } from "tc-shared/events";
import { ReactComponentBase } from "tc-shared/ui/react-elements/ReactComponentBase";
import { FileBrowserEvents } from "tc-shared/ui/modal/transfer/FileDefinitions";
import React = require("react");
export interface FileBrowserRendererClasses {
    navigation?: {
        boxedInput?: string;
    };
    fileTable?: {
        table?: string;
        header?: string;
        body?: string;
    };
    fileEntry?: {
        entry?: string;
        selected?: string;
        dropHovered?: string;
    };
}
export declare const FileBrowserClassContext: React.Context<FileBrowserRendererClasses>;
interface NavigationBarProperties {
    initialPath: string;
    events: Registry<FileBrowserEvents>;
}
interface NavigationBarState {
    currentPath: string;
    state: "editing" | "navigating" | "normal";
}
export declare class NavigationBar extends ReactComponentBase<NavigationBarProperties, NavigationBarState> {
    private refRendered;
    private refInput;
    private ignoreBlur;
    private lastSucceededPath;
    protected defaultState(): NavigationBarState;
    componentDidMount(): void;
    render(): JSX.Element;
    componentDidUpdate(prevProps: Readonly<NavigationBarProperties>, prevState: Readonly<NavigationBarState>, snapshot?: any): void;
    private onPathClicked;
    private onRenderedPathClicked;
    private onInputPathBluer;
    private onPathEntered;
    private onButtonRefreshClicked;
    private handleNavigateBegin;
    private handleCurrentPath;
}
interface FileListTableProperties {
    initialPath: string;
    events: Registry<FileBrowserEvents>;
}
interface FileListTableState {
    state: "querying" | "normal" | "error" | "query-timeout" | "no-permissions" | "invalid-password";
    errorMessage?: string;
}
export declare class FileBrowserRenderer extends ReactComponentBase<FileListTableProperties, FileListTableState> {
    private refTable;
    private currentPath;
    private fileList;
    private selection;
    protected defaultState(): FileListTableState;
    render(): JSX.Element;
    componentDidMount(): void;
    private onDrop;
    private onHeaderContextMenu;
    private onBodyContextMenu;
    private handleNavigationResult;
    private handleQueryFiles;
    private handleQueryFilesResult;
    private handleActionDeleteResult;
    private handleActionFileCreateBegin;
    private handleActionFileCreateResult;
    private handleActionSelectFiles;
    private handleNotifyDragStarted;
    private handleFileRenameResult;
    private handleTransferStart;
    private handleTransferStatus;
}
export {};
