export interface AddressTarget {
    target_ip: string;
    target_port?: number;
}
export interface ResolveOptions {
    timeout?: number;
    allowCache?: boolean;
    maxDepth?: number;
    allowSrv?: boolean;
    allowCName?: boolean;
    allowAny?: boolean;
    allowA?: boolean;
    allowAAAA?: boolean;
}
export declare const default_options: ResolveOptions;
export interface DNSResolveOptions {
    timeout?: number;
    allowCache?: boolean;
    maxDepth?: number;
    allowSrv?: boolean;
    allowCName?: boolean;
    allowAny?: boolean;
    allowA?: boolean;
    allowAAAA?: boolean;
}
export interface DNSAddress {
    hostname: string;
    port: number;
}
export declare type DNSResolveResult = {
    status: "success";
    originalAddress: DNSAddress;
    resolvedAddress: DNSAddress;
} | {
    status: "error";
    message: string;
} | {
    status: "empty-result";
};
export interface DNSProvider {
    resolveAddress(address: DNSAddress, options: DNSResolveOptions): Promise<DNSResolveResult>;
    resolveAddressIPv4(address: DNSAddress, options: DNSResolveOptions): Promise<DNSResolveResult>;
}
export declare function getDNSProvider(): DNSProvider;
export declare function setDNSProvider(newProvider: DNSProvider): void;
