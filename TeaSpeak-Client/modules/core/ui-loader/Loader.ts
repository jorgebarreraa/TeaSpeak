import {is_debug} from "../main-window";
import * as moment from "moment";
import * as fs from "fs-extra";
import * as os from "os";

import * as path from "path";
import * as zlib from "zlib";
import * as tar from "tar-stream";
import {Arguments, processArguments} from "../../shared/process-arguments";
import {parseVersion} from "../../shared/version";

import {clientAppInfo, currentClientVersion} from "../app-updater";
import {CachedUIPack, UIPackInfo} from "./CacheFile";
import {localUiCache, saveLocalUiCache} from "./Cache";
import {shippedClientUi} from "./Shipped";
import {downloadUiPack, queryRemoteUiPacks} from "./Remote";
import {protocol} from "electron";

const kUiPackProtocol = "shared-ui";

export const remoteUiUrl = () => {
    return processArguments.has_value(...Arguments.SERVER_URL) ? processArguments.value(...Arguments.SERVER_URL) : "https://clientapi.teaspeak.de/";
};

export function initializeCustomUiPackProtocol() {
    protocol.registerSchemesAsPrivileged([{
        scheme: kUiPackProtocol,
        privileges: {
            allowServiceWorkers: true,
            supportFetchAPI: true,
            bypassCSP: false,
            corsEnabled: false,
            secure: true,
            standard: true
        }
    }]);
}

let temporaryDirectoryPromise: Promise<string>;
function generateTemporaryDirectory() : Promise<string> {
    if(temporaryDirectoryPromise) {
        return temporaryDirectoryPromise;
    }

    return (temporaryDirectoryPromise = fs.mkdtemp(path.join(os.tmpdir(), "TeaClient-")).then(path => {
        process.on('exit', () => {
            try {
                if(fs.pathExistsSync(path)) {
                    fs.removeSync(path);
                }
            } catch (e) {
                console.warn("Failed to delete temp directory: %o", e);
            }
        });
        
        global["browser-root"] = path;
        console.log("Local browser path: %s", path);
        return path;
    }));
}

async function unpackLocalUiPack(version: CachedUIPack) : Promise<string> {
    const targetDirectory = await generateTemporaryDirectory();
    if(!await fs.pathExists(targetDirectory)) {
        throw "failed to create temporary directory";
    }

    const unzip = zlib.createUnzip();
    const extract = tar.extract();
    let fpipe: fs.ReadStream;

    try {
        fpipe = fs.createReadStream(version.localFilePath);
    } catch (error) {
        console.error("Failed to open UI pack at %s: %o", version.localFilePath, error);
        throw "failed to open UI pack";
    }

    extract.on('entry', function(header: tar.Headers, stream, next) {
        if(header.type == 'file') {
            const targetFile = path.join(targetDirectory, header.name);
            if(!fs.existsSync(path.dirname(targetFile))) {
                fs.mkdirsSync(path.dirname(targetFile));
            }

            stream.on('end', () => setImmediate(next));
            const wfpipe = fs.createWriteStream(targetFile);
            stream.pipe(wfpipe);
        } else if(header.type == 'directory') {
            if(fs.existsSync(path.join(targetDirectory, header.name))) {
                setImmediate(next);
            }

            fs.mkdirs(path.join(targetDirectory, header.name)).catch(error => {
                console.warn("Failed to create unpacking dir " + path.join(targetDirectory, header.name));
                console.error(error);
            }).then(() => setImmediate(next));
        } else {
            console.warn("Invalid ui tar ball entry type (" + header.type + ")");
            return;
        }
    });

    const finishPromise = new Promise((resolve, reject) => {
        unzip.on('error', event => {
            reject(event);
        });

        extract.on('finish', resolve);
        extract.on('error', event => {
            if(!event) return;
            reject(event);
        });

        fpipe.pipe(unzip).pipe(extract);
    });

    try {
        await finishPromise;
    } catch(error) {
        console.error("Failed to extract UI files to %s: %o", targetDirectory, error);
        throw "failed to unpack the UI pack";
    }

    return targetDirectory;
}

let uiFilePath: string;
export function getUiFilePath() : string | undefined {
    return uiFilePath;
}

function initializeFileSystemProtocol(directory: string) {
    directory = path.normalize(directory);
    uiFilePath = directory;

    protocol.registerFileProtocol(kUiPackProtocol, (request, callback) => {
        let targetPath;
        const pathStartIndex = request.url.indexOf('/', kUiPackProtocol.length + 3);
        if(pathStartIndex === -1) {
            targetPath = path.join(directory, "index.html");
        } else {
            const endIndex = request.url.indexOf("?");

            let requestedPath = endIndex === -1 ? request.url.substring(pathStartIndex) : request.url.substring(pathStartIndex, endIndex);
            targetPath = path.normalize(path.join(directory, requestedPath));
        }

        if(!targetPath.startsWith(directory)) {
            console.warn("Having UI-Pack path request which exceeds the target directory: %s", targetPath);
            callback({ path: "__non__exiting" });
        } else {
            console.debug("Resolved %s to %s", request.url, targetPath);
            callback({ path: targetPath });
        }
    });
}

async function streamFilesFromDevServer(_channel: string, _callbackStatus: (message: string, index: number) => any) : Promise<string> {
    return remoteUiUrl() + "index.html";
}

async function loadBundledUiPack(channel: string, callbackStatus: (message: string, index: number) => any) : Promise<string> {
    callbackStatus("Query local UI pack info", .33);

    const bundledUi = await shippedClientUi();
    if(!bundledUi) {
        throw "client has no bundled UI pack";
    }

    callbackStatus("Unpacking bundled UI", .66);
    const result = await unpackLocalUiPack(bundledUi);

    callbackStatus("Local UI pack loaded", 1);
    console.log("Loaded bundles UI pack successfully. Version: {timestamp: %d, hash: %s}", bundledUi.packInfo.timestamp, bundledUi.packInfo.versions_hash);
    initializeFileSystemProtocol(result);
    return kUiPackProtocol + "://shared-ui/index.html";
}

async function loadCachedOrRemoteUiPack(channel: string, callbackStatus: (message: string, index: number) => any, ignoreNewVersionTimestamp: boolean) : Promise<string> {
    callbackStatus("Fetching info", 0);

    const bundledUi = await shippedClientUi();
    const clientVersion = await currentClientVersion();

    console.error("Looking for downloaded up packs on channel %s", channel);
    let availableCachedVersions: CachedUIPack[] = localUiCache().cachedPacks.filter(e => {
        if(e.status.type !== "valid") {
            console.error("Not valid");
            return false;
        }

        if(e.packInfo.channel !== channel) {
            console.error("%s !== %s", e.packInfo.channel, channel);
            /* ui-pack is for another channel */
            return false;
        }

        if(bundledUi) {
            /* remove all cached ui packs which are older than our bundled one */
            if(e.packInfo.timestamp <= bundledUi.packInfo.timestamp) {
                console.error("%d <= %s", e.packInfo.timestamp, bundledUi.packInfo.timestamp);
                return false;
            }
        }

        const requiredVersion = parseVersion(e.packInfo.requiredClientVersion);
        return clientVersion.isDevelopmentVersion() || clientVersion.newerThan(requiredVersion, true) || clientVersion.equals(requiredVersion);
    });

    if(processArguments.has_flag(Arguments.UPDATER_UI_NO_CACHE)) {
        console.log("Ignoring local UI cache");
        availableCachedVersions = [];
    }

    console.error("Found %d local UI packs (%d not suitable).", availableCachedVersions.length, localUiCache().cachedPacks.length - availableCachedVersions.length);
    let remoteVersionDropped = false;

    /* fetch the remote versions  */
    executeRemoteLoader: {
        callbackStatus("Loading remote info", .25);

        let remoteVersions: UIPackInfo[];
        try {
            remoteVersions = await queryRemoteUiPacks();
        } catch (error) {
            console.error("Failed to query remote UI packs: %o", error);
            break executeRemoteLoader;
        }

        callbackStatus("Parsing remote UI packs", .40);
        const remoteVersion = remoteVersions.find(e => e.channel === channel);
        if(!remoteVersion) {
            console.info("Remote server has no ui packs for channel %o.", channel);
            break executeRemoteLoader;
        }

        let newestLocalVersion = availableCachedVersions.map(e => e.packInfo.timestamp)
            .reduce((a, b) => Math.max(a, b), bundledUi ? bundledUi.downloadTimestamp : 0);

        console.log("Remote version %d, Local version %d", remoteVersion.timestamp, newestLocalVersion);
        const requiredClientVersion = parseVersion(remoteVersion.requiredClientVersion);
        if(requiredClientVersion.newerThan(clientVersion) && !is_debug) {
            /* We can't use the newer version. Use the latest available. Update prompt should come when starting the client */
            console.log("Ignoring remote version since our client is too old to use it. Required client: %s, Client version: %s", remoteVersion.requiredClientVersion, clientVersion.toString());
        } else if(remoteVersion.timestamp <= newestLocalVersion && !ignoreNewVersionTimestamp) {
            /* We've already a equal or newer version. Don't use the remote version */
            /* if remote is older than current bundled version its not a drop since it could be used as a fallback */
            remoteVersionDropped = !!bundledUi && remoteVersion.timestamp > bundledUi.downloadTimestamp;
        } else {
            /* update is possible because the timestamp is newer than out latest local version */
            try {
                console.log("Downloading UI pack version (%d) %s. Forced: %s. Newest local version: %d", remoteVersion.timestamp,
                    remoteVersion.versions_hash, ignoreNewVersionTimestamp ? "true" : "false", newestLocalVersion);

                callbackStatus("Downloading new UI pack", .55);
                availableCachedVersions.push(await downloadUiPack(remoteVersion));
            } catch (error) {
                console.error("Failed to download new UI pack: %o", error);
            }
        }
    }

    callbackStatus("Unpacking UI", .70);
    availableCachedVersions.sort((a, b) => a.packInfo.timestamp - b.packInfo.timestamp);

    /* Only invalidate the version if any other succeeded to load else we might fucked up (no permission to write etc) */
    let invalidatedVersions: CachedUIPack[] = [];
    const doVersionInvalidate = async () => {
        if(invalidatedVersions.length > 0) {
            for(const version of invalidatedVersions) {
                version.status = { type: "invalid", reason: "failed to unpack", timestamp: Date.now() };
            }

            await saveLocalUiCache();
        }
    };

    while(availableCachedVersions.length > 0) {
        const pack = availableCachedVersions.pop();
        console.log("Trying to load UI pack from %s (%s). Downloaded at %s",
            moment(pack.packInfo.timestamp).format("llll"), pack.packInfo.versions_hash,
            moment(pack.downloadTimestamp).format("llll"));

        try {
            const target = await unpackLocalUiPack(pack);
            callbackStatus("UI pack loaded", 1);
            await doVersionInvalidate();

            initializeFileSystemProtocol(target);
            return kUiPackProtocol + "://shared-ui/index.html";
        } catch (error) {
            invalidatedVersions.push(pack);
            console.log("Failed to unpack UI pack: %o", error);
        }
    }

    if(remoteVersionDropped) {
        /* try again, but this time enforce a remote download */
        const result = await loadCachedOrRemoteUiPack(channel, callbackStatus, true);
        await doVersionInvalidate(); /* new UI pack seems to be successfully loaded */
        return result; /* if not succeeded an exception will be thrown */
    }

    throw "Failed to load any UI pack (local and remote)\nView the console for more details.";
}

enum UILoaderMethod {
    PACK = 0,
    BUNDLED_PACK = 1,
    /* RAW_FILES = 2, System deprecated */
    DEVELOP_SERVER = 3
}

/**
 * @param statisticsCallback
 * @returns the url of the ui pack entry point
 */
export async function loadUiPack(statisticsCallback: (message: string, index: number) => any) : Promise<string> {
    const channel = clientAppInfo().uiPackChannel;
    let enforcedLoadingMethod = parseInt(processArguments.has_value(Arguments.UPDATER_UI_LOAD_TYPE) ? processArguments.value(Arguments.UPDATER_UI_LOAD_TYPE) : "-1") as UILoaderMethod;

    if(typeof UILoaderMethod[enforcedLoadingMethod] !== "undefined") {
        switch (enforcedLoadingMethod) {
            case UILoaderMethod.PACK:
                return await loadCachedOrRemoteUiPack(channel, statisticsCallback, false);

            case UILoaderMethod.BUNDLED_PACK:
                return await loadBundledUiPack(channel, statisticsCallback);

            case UILoaderMethod.DEVELOP_SERVER:
                return await streamFilesFromDevServer(channel, statisticsCallback);

            default:
                console.warn("Invalid ui loader type %o. Skipping loader enforcement.", enforcedLoadingMethod);
        }
    }

    let firstError;
    try {
        return await loadCachedOrRemoteUiPack(channel, statisticsCallback, false);
    } catch(error) {
        console.warn("Failed to load cached/remote UI pack: %o", error);
        firstError = firstError || error;
    }

    try {
        return await loadBundledUiPack(channel, statisticsCallback);
    } catch(error) {
        console.warn("Failed to load bundles UI pack: %o", error);
        firstError = firstError || error;
    }

    throw firstError;
}