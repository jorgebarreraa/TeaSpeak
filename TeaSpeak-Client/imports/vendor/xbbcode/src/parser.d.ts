import { Element } from "./elements";
import * as Registry from "./registry";
export interface Options {
    maxDepth?: number;
    tagRegistry?: Registry.TagRegistry;
    lazyCloseTag?: boolean;
    tag_blacklist?: string[] | undefined;
    tag_whitelist?: string[] | undefined;
    enforce_back_whitelist?: boolean;
    verbose?: boolean;
}
export declare function parse(text: string, options_: Options): Element[];
