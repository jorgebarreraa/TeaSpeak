import { ClientIcon } from "svg-sprites/client-icons";
import { RemoteIconInfo } from "tc-shared/file/Icons";
export declare type MenuBarEntrySeparator = {
    uniqueId?: string;
    type: "separator";
};
export declare type MenuBarEntryNormal = {
    uniqueId?: string;
    type: "normal";
    label: string;
    disabled?: boolean;
    visible?: boolean;
    icon?: ClientIcon | RemoteIconInfo;
    click?: () => void;
    children?: MenuBarEntry[];
};
export declare type MenuBarEntry = MenuBarEntrySeparator | MenuBarEntryNormal;
export interface MenuBarDriver {
    /**
     * Separators on top level might not be rendered.
     * @param entries
     */
    setEntries(entries: MenuBarEntry[]): any;
    /**
     * Removes the menu bar
     */
    clearEntries(): any;
}
export declare function getMenuBarDriver(): MenuBarDriver;
export declare function setMenuBarDriver(driver_: MenuBarDriver): void;
