import { KeyDescriptor } from "./PPTListener";
export interface KeyControl {
    category: string;
    description: string;
    handler: () => void;
    icon: string;
}
export declare const TypeCategories: {
    [key: string]: {
        name: string;
    };
};
export declare const KeyTypes: {
    [key: string]: KeyControl;
};
export declare function initializeKeyControl(): void;
export declare function setKey(action: string, key?: KeyDescriptor): void;
export declare function key(action: string): KeyDescriptor | undefined;
