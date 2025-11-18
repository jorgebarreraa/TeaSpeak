export declare class Crc32 {
    private static readonly lookup;
    private crc;
    constructor();
    update(data: ArrayBufferLike): void;
    digest(radix: number): string;
}
