export declare enum ColloquialFormat {
    YESTERDAY = 0,
    TODAY = 1,
    GENERAL = 2
}
export declare function same_day(a: number | Date, b: number | Date): boolean;
export declare function date_format(date: Date, now: Date, ignore_settings?: boolean): ColloquialFormat;
export declare function formatDayTime(date: Date): string;
export declare function format_date_general(date: Date, hours?: boolean): string;
export declare function format_date_colloquial(date: Date, current_timestamp: Date): {
    result: string;
    format: ColloquialFormat;
};
export declare function format_chat_time(date: Date): {
    result: string;
    next_update: number;
};
