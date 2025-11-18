import {NativeImage} from "electron";

export interface NativeMenuBarEntry {
    uniqueId: string,
    type: "separator" | "normal",
    label?: string,
    icon?: string,
    disabled?: boolean,
    children?: NativeMenuBarEntry[]
}