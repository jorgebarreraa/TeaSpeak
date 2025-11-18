import * as React from "react";
import { RemoteIconInfo } from "tc-shared/file/Icons";
export interface DropdownEntryProperties {
    icon?: string | RemoteIconInfo;
    text: JSX.Element | string;
    onClick?: (event: React.MouseEvent) => void;
    onAuxClick?: (event: React.MouseEvent) => void;
    onContextMenu?: (event: React.MouseEvent) => void;
    children?: React.ReactElement<DropdownEntry | DropdownTitleEntry>[];
}
export declare class DropdownEntry extends React.PureComponent<DropdownEntryProperties> {
    render(): JSX.Element;
}
export declare class DropdownTitleEntry extends React.PureComponent<{
    children: any;
}> {
    render(): JSX.Element;
}
export declare const DropdownContainer: (props: {
    children: any;
}) => JSX.Element;
