export declare enum ChatType {
    GENERAL = 0,
    SERVER = 1,
    CHANNEL = 2,
    CLIENT = 3
}
export declare function htmlEscape(message: string): string[];
export declare function formatElement(object: any, escape_html?: boolean): JQuery[];
export declare function formatMessage(pattern: string, ...objects: any[]): JQuery[];
export declare function formatMessageString(pattern: string, ...args: (string | number | boolean)[]): string;
export declare function parseMessageWithArguments(pattern: string, argumentCount: number): (string | number)[];
export declare namespace network {
    const KiB = 1024;
    const MiB: number;
    const GiB: number;
    const TiB: number;
    const kB = 1000;
    const MB: number;
    const GB: number;
    const TB: number;
    function binarySizeToString(value: number): string;
    function decimalSizeToString(value: number): string;
    function format_bytes(value: number, options?: {
        time?: string;
        unit?: string;
        exact?: boolean;
    }): string;
}
export declare const K = 1000;
export declare const M: number;
export declare const G: number;
export declare const T: number;
export declare function format_number(value: number, options?: {
    time?: string;
    unit?: string;
}): string;
export declare const TIME_SECOND = 1000;
export declare const TIME_MINUTE: number;
export declare const TIME_HOUR: number;
export declare const TIME_DAY: number;
export declare const TIME_WEEK: number;
export declare function format_time(time: number, default_value: string): string;
export declare function set_icon_size(size: string): void;
