interface GeoLocationInfo {
    country: string;
    city?: string;
    region?: string;
    timezone?: string;
}
declare class GeoLocationProvider {
    private readonly resolver;
    private currentResolverIndex;
    private cachedInfo;
    private lookupPromise;
    constructor();
    loadCache(): void;
    private doLoadCache;
    queryInfo(timeout: number): Promise<GeoLocationInfo | undefined>;
    private doQueryInfo;
}
export declare let geoLocationProvider: GeoLocationProvider;
export {};
