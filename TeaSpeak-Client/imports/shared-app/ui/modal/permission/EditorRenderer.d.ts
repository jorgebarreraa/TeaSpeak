import * as React from "react";
import { Registry } from "tc-shared/events";
import { PermissionEditorEvents } from "tc-shared/ui/modal/permission/EditorDefinitions";
interface PermissionEditorProperties {
    handlerId: string;
    serverUniqueId: string;
    events: Registry<PermissionEditorEvents>;
}
interface PermissionEditorState {
    state: "no-permissions" | "unset" | "normal";
}
export declare class EditorRenderer extends React.Component<PermissionEditorProperties, PermissionEditorState> {
    render(): JSX.Element;
    componentDidMount(): void;
}
export {};
