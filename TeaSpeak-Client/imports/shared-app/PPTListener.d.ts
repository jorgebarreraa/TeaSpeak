export declare enum EventType {
    KEY_PRESS = 0,
    KEY_RELEASE = 1,
    KEY_TYPED = 2
}
export declare enum SpecialKey {
    CTRL = 0,
    WINDOWS = 1,
    SHIFT = 2,
    ALT = 3
}
export interface KeyDescriptor {
    keyCode: string;
    keyCtrl: boolean;
    keyWindows: boolean;
    keyShift: boolean;
    keyAlt: boolean;
}
export interface KeyEvent extends KeyDescriptor {
    readonly type: EventType;
    readonly key: string;
}
export interface KeyHook extends Partial<KeyDescriptor> {
    callbackPress: () => any;
    callbackRelease: () => any;
}
interface RegisteredKeyHook extends KeyHook {
    triggered: boolean;
}
export interface KeyBoardBackend {
    registerListener(listener: (event: KeyEvent) => void): any;
    unregisterListener(listener: (event: KeyEvent) => void): any;
    registerHook(hook: KeyHook): () => void;
    unregisterHook(hook: KeyHook): any;
    isKeyPressed(key: string | SpecialKey): boolean;
}
export declare class AbstractKeyBoard implements KeyBoardBackend {
    protected readonly registeredListener: ((event: KeyEvent) => void)[];
    protected readonly activeSpecialKeys: {
        [key: number]: boolean;
    };
    protected readonly activeKeys: any;
    protected registeredKeyHooks: RegisteredKeyHook[];
    constructor();
    protected destroy(): void;
    isKeyPressed(key: string | SpecialKey): boolean;
    registerHook(hook: KeyHook): () => void;
    unregisterHook(hook: KeyHook): void;
    registerListener(listener: (event: KeyEvent) => void): void;
    unregisterListener(listener: (event: KeyEvent) => void): void;
    private shouldHookBeActive;
    protected fireKeyEvent(event: KeyEvent): void;
    protected resetKeyboardState(): void;
}
export declare function getKeyBoard(): KeyBoardBackend;
export declare function setKeyBoardBackend(newBackend: KeyBoardBackend): void;
export declare function getKeyDescription(key: KeyDescriptor): string;
export {};
