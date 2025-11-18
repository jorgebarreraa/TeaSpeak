export declare class Stream {
    private static HEX_DIGITS;
    private static reTimeS;
    private static reTimeL;
    position: number;
    data: string | ArrayBuffer;
    constructor(data: string | Stream | ArrayBuffer, position: number);
    length(): number;
    get(position?: number): any;
    hexByte(byte: number): string;
    parseStringISO(start: any, end: any): string;
    parseStringUTF(start: any, end: any): string;
    parseStringBMP(start: any, end: any): string;
    parseTime(start: any, end: any, shortYear: any): string;
    parseInteger(start: any, end: any): string;
    isASCII(start: number, end: number): boolean;
    parseBitString(start: any, end: any, maxLength: any): string;
    parseOctetString(start: any, end: any, maxLength: any): any;
    parseOID(start: any, end: any, maxLength: any): any;
}
export declare enum TagClass {
    UNIVERSAL = 0,
    APPLICATION = 1,
    CONTEXT = 2,
    PRIVATE = 3
}
export declare enum TagType {
    EOC = 0,
    BOOLEAN = 1,
    INTEGER = 2,
    BIT_STRING = 3,
    OCTET_STRING = 4,
    NULL = 5,
    OBJECT_IDENTIFIER = 6,
    ObjectDescriptor = 7,
    EXTERNAL = 8,
    REAL = 9,
    ENUMERATED = 10,
    EMBEDDED_PDV = 11,
    UTF8String = 12,
    SEQUENCE = 16,
    SET = 17,
    NumericString = 18,
    PrintableString = 19,
    TeletextString = 20,
    VideotexString = 21,
    IA5String = 22,
    UTCTime = 23,
    GeneralizedTime = 24,
    GraphicString = 25,
    VisibleString = 26,
    GeneralString = 27,
    UniversalString = 28,
    BMPString = 30
}
declare class ASN1Tag {
    tagClass: TagClass;
    type: TagType;
    tagConstructed: boolean;
    tagNumber: number;
    constructor(stream: Stream);
    isUniversal(): boolean;
    isEOC(): boolean;
}
export declare class ASN1 {
    stream: Stream;
    header: number;
    length: number;
    tag: ASN1Tag;
    children: ASN1[];
    constructor(stream: Stream, header: number, length: number, tag: ASN1Tag, children: ASN1[]);
    content(max_length?: number, type?: TagType): any;
    typeName(): string;
    toString(): string;
    toPrettyString(indent: any): string;
    posStart(): number;
    posContent(): number;
    posEnd(): number;
    static decodeLength(stream: Stream): any;
    static encodeLength(buffer: Uint8Array, offset: number, length: number): void;
}
export declare function decode(stream: string | ArrayBuffer): ASN1;
export {};
