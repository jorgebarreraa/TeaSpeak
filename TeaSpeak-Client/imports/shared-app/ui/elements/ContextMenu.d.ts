export interface MenuEntry {
    callback?: () => void;
    type: MenuEntryType;
    name: (() => string) | string;
    icon_class?: string;
    icon_path?: string;
    disabled?: boolean;
    visible?: boolean;
    checkbox_checked?: boolean;
    invalidPermission?: boolean;
    sub_menu?: MenuEntry[];
}
export declare enum MenuEntryType {
    CLOSE = 0,
    ENTRY = 1,
    CHECKBOX = 2,
    HR = 3,
    SUB_MENU = 4
}
export declare class Entry {
    static HR(): {
        callback: () => void;
        type: MenuEntryType;
        name: string;
        icon: string;
    };
    static CLOSE(callback: () => void): {
        callback: () => void;
        type: MenuEntryType;
        name: string;
        icon: string;
    };
}
export interface ContextMenuProvider {
    despawn_context_menu(): any;
    spawn_context_menu(x: number, y: number, ...entries: MenuEntry[]): any;
    initialize(): any;
    finalize(): any;
    html_format_enabled(): boolean;
}
export declare function spawn_context_menu(x: number, y: number, ...entries: MenuEntry[]): void;
export declare function despawn_context_menu(): void;
export declare function get_provider(): ContextMenuProvider;
export declare function set_provider(_provider: ContextMenuProvider): void;
