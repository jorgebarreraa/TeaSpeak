import * as React from "react";
import { ReactElement } from "react";
export interface TableColumn {
    name: string;
    header: () => ReactElement | ReactElement[];
    width?: number;
    fixedWidth?: string;
    className?: string;
}
export interface TableRow<T = any> {
    columns: {
        [key: string]: () => ReactElement | ReactElement[];
    };
    className?: string;
    userData?: T;
}
export interface TableProperties {
    columns: TableColumn[];
    rows: TableRow[];
    className?: string;
    headerClassName?: string;
    bodyClassName?: string;
    bodyOverlayOnly?: boolean;
    bodyOverlay?: () => ReactElement;
    hiddenColumns?: string[];
    onHeaderContextMenu?: (event: React.MouseEvent) => void;
    onBodyContextMenu?: (event: React.MouseEvent) => void;
    onDrop?: (event: React.DragEvent) => void;
    onDragOver?: (event: React.DragEvent) => void;
    renderRow?: (row: TableRow, columns: TableColumn[], uniqueId: string) => React.ReactElement<TableRowElement>;
}
export interface TableState {
    hiddenColumns: string[];
}
export interface TableRowProperties {
    columns: TableColumn[];
    rowData: TableRow;
}
export interface TableRowState {
    hidden?: boolean;
}
export declare class TableRowElement extends React.Component<TableRowProperties & React.HTMLProps<HTMLDivElement>, TableRowState> {
    constructor(props: any);
    render(): React.DetailedReactHTMLElement<any, HTMLElement>;
}
export declare class Table extends React.Component<TableProperties, TableState> {
    private rowIndex;
    private refHeader;
    private refHiddenHeader;
    private refBody;
    private lastHeaderHeight;
    private lastScrollbarWidth;
    constructor(props: any);
    render(): JSX.Element;
    componentDidUpdate(prevProps: Readonly<TableProperties>, prevState: Readonly<TableState>, snapshot?: any): void;
}
