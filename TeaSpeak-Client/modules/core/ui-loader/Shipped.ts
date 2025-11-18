import {CachedUIPack} from "./CacheFile";
import * as fs from "fs-extra";
import * as path from "path";
import validate from "./ShippedFileInfo.validator";
import {app} from "electron";

async function doQueryShippedUi() {
    const appPath = app.getAppPath();
    if(!appPath.endsWith(".asar")) {
        return undefined;
    }

    const basePath = path.join(path.dirname(appPath), "ui");
    //console.debug("Looking for client shipped UI pack at %s", basePath);
    if(!(await fs.pathExists(basePath))) {
        return undefined;
    }

    const info = validate(await fs.readJson(path.join(basePath, "bundled-ui.json")));
    return {
        downloadTimestamp: info.timestamp * 1000,
        status: { type: "valid" },
        localChecksum: "none",
        localFilePath: path.join(path.join(path.dirname(appPath), "ui"), info.filename),
        packInfo: {
            channel: info.channel,
            requiredClientVersion: info.required_client,
            timestamp: info.timestamp * 1000,
            version: info.version,
            versions_hash: info.git_hash
        }
    };
}

let queryPromise: Promise<CachedUIPack | undefined>;

/**
 * This function will not throw.
 *
 * @returns the shipped client ui.
 *          Will return undefined if no UI has been shipped or it's an execution from source.
 */
export async function shippedClientUi() : Promise<CachedUIPack | undefined> {
    if(queryPromise) {
        return queryPromise;
    }

    return (queryPromise = doQueryShippedUi().catch(error => {
        console.warn("Failed to query shipped client ui: %o", error);
        return undefined;
    }));
}