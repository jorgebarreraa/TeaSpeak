export interface YesNoParameters {
    title: string;
    question: string;
    textYes?: string;
    textNo?: string;
    closeable?: boolean;
}
export declare function promptYesNo(properties: YesNoParameters): Promise<boolean | undefined>;
