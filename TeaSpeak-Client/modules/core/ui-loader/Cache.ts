import * as path from "path";
import * as fs from "fs-extra";
import * as electron from "electron";
import validateCacheFile from "./CacheFile.validator";

import CacheFile, {UIPackInfo} from "./CacheFile";

let localUiCacheInstance: CacheFile;
async function doLoad() {
    const file = path.join(uiCachePath(), "data.json");

    if(!(await fs.pathExists(file))) {
        console.debug("Missing UI cache file. Creating a new one.");
        /* we've no cache */
        return;
    }

    const anyData = await fs.readJSON(file);

    try {
        if(anyData["version"] !== 3) {
            throw "unsupported version " + anyData["version"];
        }

        localUiCacheInstance = validateCacheFile(anyData);
    } catch (error) {
        if(error?.message?.startsWith("CacheFile")) {
            /* We have no need to fully print the read data */
            error = "\n- " + error.message.split("\n")[0].split(", ").join("\n- ");
        } else if(error?.message) {
            error = error.message;
        } else if(typeof error !== "string") {
            console.error(error);
        }
        console.warn("Current Ui cache file seems to be invalid. Renaming it and creating a new one: %s", error);

        try {
            await fs.rename(file, path.join(uiCachePath(), "data.json." + Date.now()));
        } catch (error) {
            console.warn("Failed to invalidate old ui cache file: %o", error);
        }
    }
}

/**
 * Will not throw or return undefined!
 */
export async function loadLocalUiCache() {
    if(localUiCacheInstance) {
        throw "ui cache has already been loaded";
    }

    try {
        await doLoad();
    } catch (error) {
        console.warn("Failed to load UI cache file: %o. This will cause loss of the file content.", error);
    }

    if(!localUiCacheInstance) {
        localUiCacheInstance = {
            version: 3,
            cachedPacks: []
        }
    }
}

export function localUiCache() : CacheFile {
    if(typeof localUiCacheInstance !== "object") {
        throw "missing local ui cache";
    }

    return localUiCacheInstance;
}

/**
 * Will not throw anything
 */
export async function saveLocalUiCache() {
    const file = path.join(uiCachePath(), "data.json");
    try {
        if(!(await fs.pathExists(path.dirname(file)))) {
            await fs.mkdirs(path.dirname(file));
        }

        await fs.writeJson(file, localUiCacheInstance);
    } catch (error) {
        console.error("Failed to save UI cache file. This will may cause some data loss: %o", error);
    }
}

export function uiCachePath() {
    return path.join(electron.app.getPath('userData'), "cache", "ui");
}

export function uiPackCachePath(version: UIPackInfo) : string {
    return path.join(uiCachePath(), version.channel + "_" + version.versions_hash + "_" + version.timestamp + ".tar.gz");
}