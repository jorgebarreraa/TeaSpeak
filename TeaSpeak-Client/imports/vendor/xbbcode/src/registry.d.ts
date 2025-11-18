export interface ConditionalRule {
    tag: string;
    overriddenBy: string[];
}
export interface Tag {
    tag: string;
    synonyms?: string[];
    ignore_black_whitelist?: boolean;
    blacklistTags?: ConditionalRule[];
    whitelistTags?: string[];
    instantClose?: boolean;
}
export declare class TagRegistry {
    readonly parent: TagRegistry | undefined;
    private registeredTags;
    private tagMap;
    constructor(parent: TagRegistry | undefined);
    findTag(tag: string, normalized?: boolean): Tag;
    registerTag(tag: Tag): void;
    tags(): Tag[];
}
export declare const Default: TagRegistry;
