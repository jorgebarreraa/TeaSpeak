import { RemoteIcon } from "tc-shared/file/Icons";
import { ClientIcon } from "svg-sprites/client-icons";
export declare type MenuEntryLabel = {
    text: string;
    bold?: boolean;
} | string;
export declare type MenuEntryClickable = {
    uniqueId?: string;
    label: MenuEntryLabel;
    enabled?: boolean;
    visible?: boolean;
    click?: () => void;
    icon?: RemoteIcon | ClientIcon;
};
export declare type ContextMenuEntryNormal = {
    type: "normal";
    subMenu?: ContextMenuEntry[];
} & MenuEntryClickable;
export declare type ContextMenuEntrySeparator = {
    uniqueId?: string;
    type: "separator";
    visible?: boolean;
};
export declare type ContextMenuEntryCheckbox = {
    type: "checkbox";
    checked?: boolean;
} & MenuEntryClickable;
export declare type ContextMenuEntry = ContextMenuEntryNormal | ContextMenuEntrySeparator | ContextMenuEntryCheckbox;
export interface ContextMenuFactory {
    spawnContextMenu(position: {
        pageX: number;
        pageY: number;
    }, entries: ContextMenuEntry[], callbackClose?: () => void): any;
    closeContextMenu(): any;
}
export declare function setGlobalContextMenuFactory(instance: ContextMenuFactory): void;
export declare function spawnContextMenu(position: {
    pageX: number;
    pageY: number;
}, entries: ContextMenuEntry[], callbackClose?: () => void): void;
export declare function closeContextMenu(): void;
