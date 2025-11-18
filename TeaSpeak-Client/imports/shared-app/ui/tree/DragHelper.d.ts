import { ClientIcon } from "svg-sprites/client-icons";
import { ChannelTreeDragData, ChannelTreeDragEntry } from "tc-shared/ui/tree/Definitions";
export declare type DragImageEntryType = {
    icon: ClientIcon;
    name: string;
};
export declare function generateDragElement(entries: DragImageEntryType[]): HTMLElement;
export declare function setupDragData(transfer: DataTransfer, handlerId: string, entries: ChannelTreeDragEntry[], type: string): void;
export declare function parseDragData(transfer: DataTransfer): ChannelTreeDragData | undefined;
export declare function getDragInfo(transfer: DataTransfer): {
    handlerId: string;
    type: string;
} | undefined;
