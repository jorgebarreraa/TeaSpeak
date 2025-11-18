export interface CacheFile {
    version: number; /* currently 2 */

    cachedPacks: CachedUIPack[];
}

export interface UIPackInfo {
    timestamp: number; /* build timestamp */
    version: string; /* not really used anymore */
    versions_hash: string; /* used, identifies the version. Its the git hash. */

    channel: string;
    requiredClientVersion: string; /* minimum version from the client required for the pack */
}

export interface CachedUIPack {
    downloadTimestamp: number;

    localFilePath: string;
    localChecksum: string | "none"; /* sha512 of the locally downloaded file. */
    //TODO: Get the remote checksum and compare them instead of the local one

    packInfo: UIPackInfo;

    status: {
        type: "valid"
    } | {
        type: "invalid",

        timestamp: number,
        reason: string
    }
}

export default CacheFile;