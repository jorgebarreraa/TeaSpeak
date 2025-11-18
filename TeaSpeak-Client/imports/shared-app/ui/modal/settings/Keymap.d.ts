/// <reference types="react" />
import { KeyDescriptor } from "tc-shared/PPTListener";
export interface KeyMapEvents {
    query_keymap: {
        action: string;
        query_type: "query-selected" | "general";
    };
    query_keymap_result: {
        action: string;
        status: "success" | "error" | "timeout";
        error?: string;
        key?: KeyDescriptor;
    };
    set_keymap: {
        action: string;
        key?: KeyDescriptor;
    };
    set_keymap_result: {
        action: string;
        status: "success" | "error" | "timeout";
        error?: string;
        key?: KeyDescriptor;
    };
    set_selected_action: {
        action: string;
    };
}
export declare const KeyMapSettings: () => JSX.Element;
