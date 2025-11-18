import {CachedUIPack, UIPackInfo} from "./CacheFile";
import * as request from "request";
import {remoteUiUrl} from "./Loader";
import * as fs from "fs-extra";
import {WriteStream} from "fs";
import {localUiCache, saveLocalUiCache, uiPackCachePath} from "./Cache";
import * as querystring from "querystring";
import * as path from "path";

const kDownloadTimeout = 30_000;

export async function queryRemoteUiPacks() : Promise<UIPackInfo[]> {
    const url = remoteUiUrl() + "api.php?" + querystring.stringify({
        type: "ui-info"
    });
    console.debug("Loading UI pack information (URL: %s)", url);

    let body = await new Promise<string>((resolve, reject) => request.get(url, { timeout: kDownloadTimeout }, (error, response, body: string) => {
        if(error) {
            reject(error);
        } else if(!response) {
            reject("missing response object");
        } else if(response.statusCode !== 200) {
            reject(response.statusCode + " " + response.statusMessage);
        } else if(!body) {
            reject("missing body in response");
        } else {
            resolve(body);
        }
    }));

    let response;
    try {
        response = JSON.parse(body);
    } catch (error) {
        console.error("Received unparsable response for UI pack info. Response: %s", body);
        throw "failed to parse response";
    }

    if(!response["success"]) {
        throw "request failed: " + (response["msg"] || "unknown error");
    }

    if(!Array.isArray(response["versions"])) {
        console.error("Response object misses 'versions' tag or has an invalid value. Object: %o", response);
        throw "response contains invalid data";
    }

    let uiVersions: UIPackInfo[] = [];
    for(const entry of response["versions"]) {
        uiVersions.push({
            channel: entry["channel"],
            versions_hash: entry["git-ref"],
            version: entry["version"],
            timestamp: parseInt(entry["timestamp"]) * 1000, /* server provices that stuff in seconds */
            requiredClientVersion: entry["required_client"]
        });
    }

    return uiVersions;
}

export async function downloadUiPack(version: UIPackInfo) : Promise<CachedUIPack> {
    const targetFile = uiPackCachePath(version);
    if(await fs.pathExists(targetFile)) {
        try {
            await fs.remove(targetFile);
        } catch (error) {
            console.error("Tried to download UI version %s, but we failed to delete the old file: %o", version.versions_hash, error);
            throw "failed to remove the old file";
        }
    }

    try {
        await fs.mkdirp(path.dirname(targetFile));
    } catch (error) {
        console.error("Failed to create target UI pack download directory at %s: %o", path.dirname(targetFile), error);
        throw "failed to create target directories";
    }

    await new Promise((resolve, reject) => {
        let fstream: WriteStream;
        try {
            request.get(remoteUiUrl() + "api.php?" + querystring.stringify({
                "type": "ui-download",
                "git-ref": version.versions_hash,
                "version": version.version,
                "timestamp": Math.floor(version.timestamp / 1000), /* remote server has only the timestamp in seconds*/
                "channel": version.channel
            }), {
                timeout: kDownloadTimeout
            }).on('response', function(response: request.Response) {
                if(response.statusCode != 200)
                    reject(response.statusCode + " " + response.statusMessage);
            }).on('error', error => {
                reject(error);
            }).pipe(fstream = fs.createWriteStream(targetFile)).on('finish', () => {
                try { fstream.close(); } catch (e) { }

                resolve();
            });
        } catch (error) {
            try { fstream.close(); } catch (e) { }

            reject(error);
        }
    });

    try {
        const cache = await localUiCache();
        const info: CachedUIPack = {
            packInfo: version,
            localFilePath: targetFile,
            localChecksum: "none", //TODO!
            status: { type: "valid" },
            downloadTimestamp: Date.now()
        };
        cache.cachedPacks.push(info);
        await saveLocalUiCache();
        return info;
    } catch (error) {
        console.error("Failed to register downloaded UI pack to the UI cache: %o", error);
        throw "failed to register downloaded UI pack to the UI cache";
    }
}