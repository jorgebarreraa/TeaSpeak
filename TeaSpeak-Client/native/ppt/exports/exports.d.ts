declare module "tc-native/ppt" {
    enum KeyEventType {
        PRESS = 0,
        RELEASE = 1,
        TYPE = 2
    }

    export interface KeyEvent {
        type: KeyEventType;

        key_code: string;

        key_shift: boolean;
        key_alt: boolean;
        key_windows: boolean;
        key_ctrl: boolean;
    }

    export function RegisterCallback(_: (_: KeyEvent) => any);
    export function UnregisterCallback(_: (_: KeyEvent) => any);
}
