export declare enum ImageType {
    UNKNOWN = 0,
    BITMAP = 1,
    PNG = 2,
    GIF = 3,
    SVG = 4,
    JPEG = 5
}
export declare function imageType2MediaType(type: ImageType, file?: boolean): "svg" | "bmp" | "gif" | "svg+xml" | "jpeg" | "png";
export declare function responseImageType(encoded_data: string | ArrayBuffer, base64_encoded?: boolean): ImageType;
export declare type ImageCacheState = {
    state: "loaded";
    instance: Cache;
} | {
    state: "errored";
    reason: string;
} | {
    state: "unloaded";
};
export declare class ImageCache {
    readonly cacheName: string;
    private state;
    private constructor();
    static load(cacheName: string): Promise<ImageCache>;
    private initialize;
    private getCacheInstance;
    isPersistent(): boolean;
    reset(): Promise<void>;
    cleanup(maxAge: number): Promise<void>;
    resolveCached(key: string, maxAge?: number): Promise<Response | undefined>;
    putCache(key: string, value: Response, type?: string, headers?: {
        [key: string]: string;
    }): Promise<void>;
    delete(key: string): Promise<void>;
}
