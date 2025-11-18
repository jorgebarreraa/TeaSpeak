export declare enum FilterType {
    THRESHOLD = 0,
    VOICE_LEVEL = 1,
    STATE = 2
}
export interface FilterBase {
    readonly priority: number;
    setEnabled(flag: boolean): void;
    isEnabled(): boolean;
}
export interface MarginedFilter {
    getMarginFrames(): number;
    setMarginFrames(value: number): any;
}
export interface ThresholdFilter extends FilterBase, MarginedFilter {
    readonly type: FilterType.THRESHOLD;
    getThreshold(): number;
    setThreshold(value: number): any;
    getAttackSmooth(): number;
    getReleaseSmooth(): number;
    setAttackSmooth(value: number): any;
    setReleaseSmooth(value: number): any;
    registerLevelCallback(callback: (value: number) => void): any;
    removeLevelCallback(callback: (value: number) => void): any;
}
export interface VoiceLevelFilter extends FilterBase, MarginedFilter {
    type: FilterType.VOICE_LEVEL;
    getLevel(): number;
}
export interface StateFilter extends FilterBase {
    type: FilterType.STATE;
    setState(state: boolean): any;
    isActive(): boolean;
}
export declare type FilterTypeClass<T extends FilterType> = T extends FilterType.STATE ? StateFilter : T extends FilterType.VOICE_LEVEL ? VoiceLevelFilter : T extends FilterType.THRESHOLD ? ThresholdFilter : never;
export declare type Filter = ThresholdFilter | VoiceLevelFilter | StateFilter;
