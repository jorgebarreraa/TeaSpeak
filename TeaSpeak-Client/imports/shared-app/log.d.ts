export declare enum LogCategory {
    CHANNEL = 0,
    CHANNEL_PROPERTIES = 1,
    CLIENT = 2,
    BOOKMARKS = 3,
    SERVER = 4,
    PERMISSIONS = 5,
    GENERAL = 6,
    NETWORKING = 7,
    VOICE = 8,
    CHAT = 9,
    AUDIO = 10,
    I18N = 11,
    IPC = 12,
    IDENTITIES = 13,
    STATISTICS = 14,
    DNS = 15,
    FILE_TRANSFER = 16,
    EVENT_REGISTRY = 17,
    WEBRTC = 18,
    VIDEO = 19
}
export declare enum LogType {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4
}
export declare let enabled_mapping: Map<number, boolean>;
export declare let level_mapping: Map<LogType, boolean>;
declare enum GroupMode {
    NATIVE = 0,
    PREFIX = 1
}
export declare function initialize(defaultLogLevel: LogType): void;
export declare function logTrace(category: LogCategory, message: string, ...optionalParams: any[]): void;
export declare function logDebug(category: LogCategory, message: string, ...optionalParams: any[]): void;
export declare function logInfo(category: LogCategory, message: string, ...optionalParams: any[]): void;
export declare function logWarn(category: LogCategory, message: string, ...optionalParams: any[]): void;
export declare function logError(category: LogCategory, message: string, ...optionalParams: any[]): void;
export declare function group(level: LogType, category: LogCategory, name: string, ...optionalParams: any[]): Group;
export declare function logGroupNative(level: LogType, category: LogCategory, name: string, ...optionalParams: any[]): Group;
export declare function table(level: LogType, category: LogCategory, title: string, args: any): void;
export declare class Group {
    readonly mode: GroupMode;
    readonly level: LogType;
    readonly category: LogCategory;
    readonly enabled: boolean;
    owner: Group;
    private readonly name;
    private readonly optionalParams;
    private isCollapsed;
    private initialized;
    private logPrefix;
    constructor(mode: GroupMode, level: LogType, category: LogCategory, name: string, optionalParams: any[][], owner?: Group);
    group(level: LogType, name: string, ...optionalParams: any[]): Group;
    collapsed(flag?: boolean): this;
    log(message: string, ...optionalParams: any[]): this;
    end(): void;
    get prefix(): string;
    set prefix(prefix: string);
}
export {};
