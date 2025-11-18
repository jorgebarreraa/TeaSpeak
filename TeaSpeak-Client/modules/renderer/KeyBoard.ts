import {KeyEvent as NKeyEvent, RegisterCallback, UnregisterCallback} from "tc-native/ppt";
import {AbstractKeyBoard, EventType, KeyEvent, KeyHook, SpecialKey} from "tc-shared/PPTListener";
import {tr} from "tc-shared/i18n/localize";
import {LogCategory, logTrace} from "tc-shared/log";

export class NativeKeyBoard extends AbstractKeyBoard {
    private readonly listener;

    constructor() {
        super();
        RegisterCallback(this.listener = event => this.handleNativeKeyEvent(event));
    }

    destroy() {
        UnregisterCallback(this.listener);
    }

    private handleNativeKeyEvent(nativeEvent: NKeyEvent) {
        let type;
        switch (nativeEvent.type) {
            case 0:
                type = EventType.KEY_PRESS;
                break;

            case 1:
                type = EventType.KEY_RELEASE;
                break;

            case 2:
                type = EventType.KEY_TYPED;
                break;

            default:
                return;
        }

        const event: KeyEvent = {
            type: type,

            key: nativeEvent.key_code,
            keyCode: nativeEvent.key_code,

            keyCtrl: nativeEvent.key_ctrl,
            keyShift: nativeEvent.key_shift,
            keyAlt: nativeEvent.key_alt,
            keyWindows: nativeEvent.key_windows,
        };

        this.fireKeyEvent(event);
    }
}