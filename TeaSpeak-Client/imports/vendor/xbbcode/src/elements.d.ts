import { Tag } from "./registry";
export declare type TextPosition = {
    start: number;
    end: number;
};
export interface Element {
    textPosition: TextPosition;
}
export declare class TagElement implements Element {
    tagType: Tag | undefined;
    tag: string;
    tagNormalized: string;
    textPosition: TextPosition;
    properlyClosed: boolean;
    options: string;
    content: Element[];
    constructor(tag: string, tagType: Tag | undefined, options?: string, content?: Element[]);
    deductibleAsText(): boolean;
    deductAsText(): string;
}
export declare class TextElement implements Element {
    rawText: string;
    escapeCharacters: number[];
    textPosition: TextPosition;
    constructor(text: string, escapes: number[], positionOrBegin: number | TextPosition, end?: number);
    text(): string;
}
