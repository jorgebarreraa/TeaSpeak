import { Registry } from "./events";
export declare type RegistryValueType = boolean | number | string | object;
export declare type RegistryValueTypeNames = "boolean" | "number" | "string" | "object";
export declare type RegistryValueTypeMapping<T> = T extends boolean ? "boolean" : T extends number ? "number" : T extends string ? "string" : T extends object ? "object" : never;
export interface RegistryKey<ValueType extends RegistryValueType> {
    key: string;
    valueType: RegistryValueTypeMapping<ValueType>;
    fallbackKeys?: string | string[];
    fallbackImports?: {
        [key: string]: (value: string) => ValueType;
    };
    description?: string;
    requireRestart?: boolean;
}
export interface ValuedRegistryKey<ValueType extends RegistryValueType> extends RegistryKey<ValueType> {
    defaultValue: ValueType;
}
export declare function encodeSettingValueToString<T extends RegistryValueType>(input: T): string;
export declare function resolveSettingKey<ValueType extends RegistryValueType, DefaultType>(key: RegistryKey<ValueType>, resolver: (key: string) => string | undefined | null, defaultValue: DefaultType): ValueType | DefaultType;
export declare class UrlParameterParser {
    private readonly url;
    constructor(url: URL);
    private getParameter;
    getValue<V extends RegistryValueType, DV>(key: RegistryKey<V>, defaultValue: DV): V | DV;
    getValue<V extends RegistryValueType>(key: ValuedRegistryKey<V>, defaultValue?: V): V;
}
export declare class UrlParameterBuilder {
    private parameters;
    setValue<V extends RegistryValueType>(key: RegistryKey<V>, value: V): void;
    build(): string;
}
/**
 * Switched appended to the application via the URL.
 * TODO: Passing native client switches
 */
export declare namespace AppParameters {
    const Instance: UrlParameterParser;
    function getValue<V extends RegistryValueType, DV>(key: RegistryKey<V>, defaultValue: DV): V | DV;
    function getValue<V extends RegistryValueType>(key: ValuedRegistryKey<V>, defaultValue?: V): V;
}
export declare namespace AppParameters {
    const KEY_CONNECT_ADDRESS: RegistryKey<string>;
    const KEY_CONNECT_INVITE_REFERENCE: RegistryKey<string>;
    const KEY_CONNECT_NO_SINGLE_INSTANCE: ValuedRegistryKey<boolean>;
    const KEY_CONNECT_TYPE: ValuedRegistryKey<number>;
    const KEY_CONNECT_NICKNAME: RegistryKey<string>;
    const KEY_CONNECT_TOKEN: RegistryKey<string>;
    const KEY_CONNECT_PROFILE: RegistryKey<string>;
    const KEY_CONNECT_SERVER_PASSWORD: RegistryKey<string>;
    const KEY_CONNECT_PASSWORDS_HASHED: ValuedRegistryKey<boolean>;
    const KEY_CONNECT_CHANNEL: RegistryKey<string>;
    const KEY_CONNECT_CHANNEL_PASSWORD: RegistryKey<string>;
    const KEY_IPC_APP_ADDRESS: RegistryKey<string>;
    const KEY_IPC_CORE_PEER_ADDRESS: RegistryKey<string>;
    const KEY_MODAL_IPC_CHANNEL: RegistryKey<string>;
    const KEY_LOAD_DUMMY_ERROR: ValuedRegistryKey<boolean>;
}
export interface SettingsEvents {
    notify_setting_changed: {
        setting: string;
        mode: "global" | "server";
        oldValue: string;
        newValue: string;
        newCastedValue: any;
    };
}
export declare class Settings {
    static readonly KEY_USER_IS_NEW: ValuedRegistryKey<boolean>;
    static readonly KEY_LOG_LEVEL: RegistryKey<number>;
    static readonly KEY_DISABLE_COSMETIC_SLOWDOWN: ValuedRegistryKey<boolean>;
    static readonly KEY_DISABLE_CONTEXT_MENU: ValuedRegistryKey<boolean>;
    static readonly KEY_DISABLE_GLOBAL_CONTEXT_MENU: ValuedRegistryKey<boolean>;
    static readonly KEY_DISABLE_UNLOAD_DIALOG: ValuedRegistryKey<boolean>;
    static readonly KEY_DISABLE_VOICE: ValuedRegistryKey<boolean>;
    static readonly KEY_DISABLE_MULTI_SESSION: ValuedRegistryKey<boolean>;
    static readonly KEY_I18N_DEFAULT_REPOSITORY: ValuedRegistryKey<string>;
    static readonly KEY_CLIENT_STATE_MICROPHONE_MUTED: ValuedRegistryKey<boolean>;
    static readonly KEY_CLIENT_STATE_SPEAKER_MUTED: ValuedRegistryKey<boolean>;
    static readonly KEY_CLIENT_STATE_QUERY_SHOWN: ValuedRegistryKey<boolean>;
    static readonly KEY_CLIENT_STATE_SUBSCRIBE_ALL_CHANNELS: ValuedRegistryKey<boolean>;
    static readonly KEY_CLIENT_STATE_AWAY: ValuedRegistryKey<boolean>;
    static readonly KEY_CLIENT_AWAY_MESSAGE: ValuedRegistryKey<string>;
    static readonly KEY_FLAG_CONNECT_DEFAULT: ValuedRegistryKey<boolean>;
    static readonly KEY_CONNECT_ADDRESS: ValuedRegistryKey<string>;
    static readonly KEY_CONNECT_PROFILE: ValuedRegistryKey<string>;
    static readonly KEY_CONNECT_USERNAME: ValuedRegistryKey<string>;
    static readonly KEY_CONNECT_PASSWORD: ValuedRegistryKey<string>;
    static readonly KEY_FLAG_CONNECT_PASSWORD: ValuedRegistryKey<boolean>;
    static readonly KEY_CONNECT_HISTORY: ValuedRegistryKey<string>;
    static readonly KEY_CONNECT_SHOW_HISTORY: ValuedRegistryKey<boolean>;
    static readonly KEY_CONNECT_NO_DNSPROXY: ValuedRegistryKey<boolean>;
    static readonly KEY_CERTIFICATE_CALLBACK: ValuedRegistryKey<string>;
    static readonly KEY_SOUND_MASTER: ValuedRegistryKey<number>;
    static readonly KEY_SOUND_MASTER_SOUNDS: ValuedRegistryKey<number>;
    static readonly KEY_SOUND_VOLUMES: RegistryKey<string>;
    static readonly KEY_CHAT_FIXED_TIMESTAMPS: ValuedRegistryKey<boolean>;
    static readonly KEY_CHAT_COLLOQUIAL_TIMESTAMPS: ValuedRegistryKey<boolean>;
    static readonly KEY_CHAT_COLORED_EMOJIES: ValuedRegistryKey<boolean>;
    static readonly KEY_CHAT_HIGHLIGHT_CODE: ValuedRegistryKey<boolean>;
    static readonly KEY_CHAT_TAG_URLS: ValuedRegistryKey<boolean>;
    static readonly KEY_CHAT_ENABLE_MARKDOWN: ValuedRegistryKey<boolean>;
    static readonly KEY_CHAT_ENABLE_BBCODE: ValuedRegistryKey<boolean>;
    static readonly KEY_CHAT_IMAGE_WHITELIST_REGEX: ValuedRegistryKey<string>;
    static readonly KEY_CHAT_LAST_USED_EMOJI: ValuedRegistryKey<string>;
    static readonly KEY_SWITCH_INSTANT_CHAT: ValuedRegistryKey<boolean>;
    static readonly KEY_SWITCH_INSTANT_CLIENT: ValuedRegistryKey<boolean>;
    static readonly KEY_HOSTBANNER_BACKGROUND: ValuedRegistryKey<boolean>;
    static readonly KEY_CHANNEL_EDIT_ADVANCED: ValuedRegistryKey<boolean>;
    static readonly KEY_PERMISSIONS_SHOW_ALL: ValuedRegistryKey<boolean>;
    static readonly KEY_TEAFORO_URL: ValuedRegistryKey<string>;
    static readonly KEY_FONT_SIZE: ValuedRegistryKey<number>;
    static readonly KEY_ICON_SIZE: ValuedRegistryKey<number>;
    static readonly KEY_KEYCONTROL_DATA: ValuedRegistryKey<string>;
    static readonly KEY_LAST_INVITE_LINK_TYPE: ValuedRegistryKey<string>;
    static readonly KEY_TRANSFERS_SHOW_FINISHED: ValuedRegistryKey<boolean>;
    static readonly KEY_TRANSFER_DOWNLOAD_FOLDER: RegistryKey<string>;
    static readonly KEY_IPC_REMOTE_ADDRESS: RegistryKey<string>;
    static readonly KEY_W2G_SIDEBAR_COLLAPSED: ValuedRegistryKey<boolean>;
    static readonly KEY_VOICE_ECHO_TEST_ENABLED: ValuedRegistryKey<boolean>;
    static readonly KEY_RTC_EXTRA_VIDEO_CHANNELS: ValuedRegistryKey<number>;
    static readonly KEY_RTC_EXTRA_AUDIO_CHANNELS: ValuedRegistryKey<number>;
    static readonly KEY_RNNOISE_FILTER: ValuedRegistryKey<boolean>;
    static readonly KEY_LOADER_ANIMATION_ABORT: ValuedRegistryKey<boolean>;
    static readonly KEY_STOP_VIDEO_ON_SWITCH: ValuedRegistryKey<boolean>;
    static readonly KEY_VIDEO_SHOW_ALL_CLIENTS: ValuedRegistryKey<boolean>;
    static readonly KEY_VIDEO_FORCE_SHOW_OWN_VIDEO: ValuedRegistryKey<boolean>;
    static readonly KEY_VIDEO_AUTO_SUBSCRIBE_MODE: ValuedRegistryKey<number>;
    static readonly KEY_VIDEO_DEFAULT_MAX_WIDTH: ValuedRegistryKey<number>;
    static readonly KEY_VIDEO_DEFAULT_MAX_HEIGHT: ValuedRegistryKey<number>;
    static readonly KEY_VIDEO_DEFAULT_MAX_BANDWIDTH: ValuedRegistryKey<number>;
    static readonly KEY_VIDEO_DEFAULT_KEYFRAME_INTERVAL: ValuedRegistryKey<number>;
    static readonly KEY_VIDEO_DYNAMIC_QUALITY: ValuedRegistryKey<boolean>;
    static readonly KEY_VIDEO_DYNAMIC_FRAME_RATE: ValuedRegistryKey<boolean>;
    static readonly KEY_VIDEO_QUICK_SETUP: ValuedRegistryKey<boolean>;
    static readonly KEY_VIDEO_SPOTLIGHT_MODE: ValuedRegistryKey<number>;
    static readonly KEY_INVITE_SHORT_URL: ValuedRegistryKey<boolean>;
    static readonly KEY_INVITE_ADVANCED_ENABLED: ValuedRegistryKey<boolean>;
    static readonly KEY_MICROPHONE_LEVEL_INDICATOR: RegistryKey<boolean>;
    static readonly KEY_MICROPHONE_THRESHOLD_ATTACK_SMOOTH: ValuedRegistryKey<number>;
    static readonly KEY_MICROPHONE_THRESHOLD_RELEASE_SMOOTH: ValuedRegistryKey<number>;
    static readonly KEY_MICROPHONE_THRESHOLD_RELEASE_DELAY: ValuedRegistryKey<number>;
    static readonly KEY_SPEAKER_DEVICE_ID: RegistryKey<string>;
    static readonly KEY_UPDATER_LAST_USED_UI: RegistryKey<string>;
    static readonly KEY_UPDATER_LAST_USED_CLIENT: RegistryKey<string>;
    static readonly FN_LOG_ENABLED: (category: string) => RegistryKey<boolean>;
    static readonly FN_SEPARATOR_STATE: (separator: string) => RegistryKey<string>;
    static readonly FN_LOG_LEVEL_ENABLED: (category: string) => RegistryKey<boolean>;
    static readonly FN_INVITE_LINK_SETTING: (name: string) => RegistryKey<string>;
    static readonly FN_SERVER_CHANNEL_SUBSCRIBE_MODE: (channel_id: number) => RegistryKey<number>;
    static readonly FN_SERVER_CHANNEL_COLLAPSED: (channel_id: number) => ValuedRegistryKey<boolean>;
    static readonly FN_PROFILE_RECORD: (name: string) => RegistryKey<object>;
    static readonly FN_CHANNEL_CHAT_READ: (id: number) => RegistryKey<number>;
    static readonly FN_CLIENT_MUTED: (clientUniqueId: string) => RegistryKey<boolean>;
    static readonly FN_CLIENT_VOLUME: (clientUniqueId: string) => RegistryKey<number>;
    static readonly FN_EVENTS_NOTIFICATION_ENABLED: (event: string) => RegistryKey<boolean>;
    static readonly FN_EVENTS_LOG_ENABLED: (event: string) => RegistryKey<boolean>;
    static readonly FN_EVENTS_FOCUS_ENABLED: (event: string) => RegistryKey<boolean>;
    static readonly KEYS: any[];
    readonly events: Registry<SettingsEvents>;
    private settingsCache;
    private saveWorker;
    private updated;
    private saveState;
    constructor();
    initialize(): Promise<void>;
    getValue<V extends RegistryValueType, DV>(key: RegistryKey<V>, defaultValue: DV): V | DV;
    getValue<V extends RegistryValueType>(key: ValuedRegistryKey<V>, defaultValue?: V): V;
    setValue<T extends RegistryValueType>(key: RegistryKey<T>, value?: T): void;
    globalChangeListener<T extends RegistryValueType>(key: RegistryKey<T>, listener: (newValue: T) => void): () => void;
    private doSave;
    save(): void;
}
export declare let settings: Settings;
