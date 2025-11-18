/// <reference types="react" />
import "./bbcode.scss";
export declare const escapeBBCode: (text: string) => string;
export declare const allowedBBCodes: string[];
export interface BBCodeRenderOptions {
    convertSingleUrls: boolean;
}
export declare const BBCodeRenderer: (props: {
    message: string;
    settings: BBCodeRenderOptions;
    handlerId?: string;
}) => JSX.Element;
export declare function renderBBCodeAsJQuery(message: string, settings: BBCodeRenderOptions): JQuery[];
export declare function renderBBCodeAsText(message: string): string;
