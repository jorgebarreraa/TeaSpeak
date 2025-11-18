import * as querystring from "querystring";
import * as request from "request";
import {app, dialog} from "electron";
import * as fs from "fs-extra";
import * as ofs from "original-fs";
import * as os from "os";
import * as tar from "tar-stream";
import * as path from "path";
import * as zlib from "zlib";
import * as child_process from "child_process";
import * as progress from "request-progress";
import * as util from "util";

import {parseVersion, Version} from "../../shared/version";

import MessageBoxOptions = Electron.MessageBoxOptions;
import {Headers} from "tar-stream";
import {Arguments, processArguments} from "../../shared/process-arguments";
import * as electron from "electron";
import {PassThrough} from "stream";
import ErrnoException = NodeJS.ErrnoException;
import { default as validateUpdateConfig } from "./UpdateConfigFile.validator";
import { default as validateAppInfo } from "./AppInfoFile.validator";
import UpdateConfigFile from "./UpdateConfigFile";
import AppInfoFile from "./AppInfoFile";

export type UpdateStatsCallback = (message: string, progress: number) => void;
export type UpdateLogCallback = (type: "error" | "info", message: string) => void;

export function updateServerUrl() : string {
    return processArguments.has_value(...Arguments.SERVER_URL) ? processArguments.value(...Arguments.SERVER_URL) : "https://clientapi.teaspeak.de/";
}

export interface UpdateVersion {
    channel: string;
    platform: string,
    arch: string;
    version: Version;
}

export interface UpdateData {
    versions: UpdateVersion[];
    updater_version: UpdateVersion;
}

let remoteVersionCacheTimestamp: number;
let remoteVersionCache: Promise<UpdateData>;
export async function fetchRemoteUpdateData() : Promise<UpdateData> {
    if(remoteVersionCache && remoteVersionCacheTimestamp > Date.now() - 60 * 60 * 1000) {
        return remoteVersionCache;
    }

    /* TODO: Validate remote response schema */
    remoteVersionCacheTimestamp = Date.now();
    return (remoteVersionCache = new Promise<UpdateData>((resolve, reject) => {
        const request_url = updateServerUrl() + "/api.php?" + querystring.stringify({
            type: "update-info"
        });

        console.log("Fetching update data from: %s", request_url);
        request.get(request_url, {
            timeout: 2000
        }, (error, response, body) => {
            if(error) {
                console.error("Failed to query the update server for update information: %o", error);
                setImmediate(reject, "failed to query update server");
                return;
            }

            if(!response) {
                setImmediate(reject, "Missing response object");
                return;
            }

            if(response.statusCode !== 200) {
                setImmediate(reject, "Invalid status code (" + response.statusCode + (response.statusMessage ? "/" + response.statusMessage : "") + ")");
                return;
            }

            let data: any;
            try {
                data = JSON.parse(body);
            } catch (_error) {
                setImmediate(reject, "Failed to parse response");
                return;
            }

            if(!data["success"]) {
                setImmediate(reject, "Action failed (" + (data["msg"] || "unknown error") + ")");
                return;
            }

            let resp: UpdateData = {} as any;
            resp.versions = [];
            for(const channel of Object.keys(data)) {
                if(channel == "success") continue;

                for(const entry of data[channel]) {
                    let version: UpdateVersion = {} as any;
                    version.channel = channel;
                    version.arch = entry["arch"];
                    version.platform = entry["platform"];
                    version.version = new Version(entry["version"]["major"], entry["version"]["minor"], entry["version"]["patch"], entry["version"]["build"], entry["version"]["timestamp"]);
                    if(version.channel == 'updater') {
                        resp.updater_version = version;
                    } else {
                        resp.versions.push(version);
                    }
                }
            }

            setImmediate(resolve, resp);
        });
    })).catch(error => {
        /* Don't cache errors */
        remoteVersionCache = undefined;
        remoteVersionCacheTimestamp = undefined;
        return Promise.reject(error);
    });
}

export async function availableRemoteChannels() : Promise<string[]> {
    const versions = (await fetchRemoteUpdateData()).versions.map(e => e.channel);

    return [...new Set(versions)];
}

export async function newestRemoteClientVersion(channel: string) : Promise<UpdateVersion | undefined> {
    const data = await fetchRemoteUpdateData();

    let currentVersion: UpdateVersion;
    for(const version of data.versions) {
        if(version.arch == os.arch() && version.platform == os.platform()) {
            if(version.channel == channel) {
                if(!currentVersion || version.version.newerThan(currentVersion.version)) {
                    currentVersion = version;
                }
            }
        }
    }

    return currentVersion;
}

function getAppDataDirectory() : string {
    return electron.app.getPath('userData');
}

function generateUpdateFilePath(channel: string, version: Version) : string {
    let directory = fs.realpathSync(getAppDataDirectory());

    const name = channel + "_" + version.major + "_" + version.minor + "_" + version.patch + "_" + version.build + ".tar";
    return path.join(directory, "app_versions", name);
}

export interface ProgressState {
    percent: number, // Overall percent (between 0 to 1)
    speed: number,  // The download speed in bytes/sec
    size: {
        total: number, // The total payload size in bytes
        transferred: number// The transferred payload size in bytes
    },
    time: {
        elapsed: number,// The total elapsed seconds since the start (3 decimals)
        remaining: number // The remaining seconds to finish (3 decimals)
    }
}

export async function downloadClientVersion(channel: string, version: Version, status: (state: ProgressState) => any, callbackLog: UpdateLogCallback) : Promise<string> {
    const targetFilePath = generateUpdateFilePath(channel, version);

    if(fs.existsSync(targetFilePath)) {
        callbackLog("info", "Removing old update file located at " + targetFilePath);

        /* TODO test if this file is valid and can be used */
        try {
            await fs.remove(targetFilePath);
        } catch(error) {
            throw "Failed to remove old file: " + error;
        }
    }

    try {
        await fs.mkdirp(path.dirname(targetFilePath));
    } catch(error) {
        throw "Failed to make target directory: " + path.dirname(targetFilePath);
    }

    const requestUrl = updateServerUrl() + "/api.php?" + querystring.stringify({
        type: "update-download",
        platform: os.platform(),
        arch: os.arch(),
        version: version.toString(),
        channel: channel
    });

    callbackLog("info", "Downloading version " + version.toString(false) + " to " + targetFilePath + " from " + updateServerUrl());
    console.log("Downloading update from %s. (%s)", updateServerUrl(), requestUrl);

    return new Promise<string>((resolve, reject) => {
        let fired = false;
        const fireFailed = (reason: string) => {
            if(fired) { return; }
            fired = true;

            setImmediate(reject, reason);
        };

        let stream = progress(request.get(requestUrl, {
            timeout: 10_000
        }, (error, response, _body) => {
            if(error) {
                console.error("Failed to download new client version: %o", error);
                fireFailed("Download failed");
                return;
            }

            if(!response) {
                fireFailed("Missing response object");
                return;
            }

            if(response.statusCode != 200) {
                fireFailed("Invalid HTTP response code: " + response.statusCode + (response.statusMessage ? "/" + response.statusMessage : ""));
                return;
            }
        })).on('progress', status).on('error', error => {
            console.warn("Encountered error within download pipe. Ignoring error: %o", error);
        }).on('end', function () {
            callbackLog("info", "Update downloaded.");
            console.log("Update downloaded successfully. Waiting for write stream to finish.");

            if(status) {
                status({
                    percent: 1,
                    speed: 0,
                    size: { total: 0, transferred: 0},
                    time: { elapsed: 0, remaining: 0}
                });
            }
        });

        console.log("Decompressing update package while streaming!");
        stream = stream.pipe(zlib.createGunzip());
        stream.pipe(fs.createWriteStream(targetFilePath, {
            autoClose: true
        })).on('finish', () => {
            console.log("Write stream has finished. Download successfully.");
            if(!fired && (fired = true)) {
                setImmediate(resolve, targetFilePath);
            }
        }).on('error', error => {
            console.log("Write stream encountered an error while downloading update. Error: %o", error);
            fireFailed("disk write error");
        });
    });
}

if(typeof(String.prototype.trim) === "undefined")
{
    String.prototype.trim = function()
    {
        return String(this).replace(/^\s+|\s+$/g, '');
    };
}

export async function ensureTargetFilesAreWriteable(updateFile: string) : Promise<string[]> {
    const originalFs = require('original-fs');
    if(!fs.existsSync(updateFile)) {
        throw "Missing update file (" + updateFile + ")";
    }

    let parentPath = await fs.realpath(path.dirname(app.getPath("exe")));
    const testAccess = async (file: string, mode: number) => {
        return await new Promise<NodeJS.ErrnoException>(resolve => originalFs.access(file, mode, resolve));
    };

    let code = await testAccess(updateFile, originalFs.constants.R_OK);
    if(code) {
        throw "Failed test read for update file. (" + updateFile + " results in " + code.code + ")";
    }

    const fstream = originalFs.createReadStream(updateFile);
    const tar_stream = tar.extract();

    const errors: string[] = [];
    const tester = async (header: Headers) => {
        const entryPath = path.normalize(path.join(parentPath, header.name));
        if(header.type == "file") {
            if(originalFs.existsSync(entryPath)) {
                code = await testAccess(entryPath, originalFs.constants.W_OK);
                if(code) {
                    errors.push("Failed to acquire write permissions for file " + entryPath + " (Code " + code.code + ")");
                }
            } else {
                let directory = path.dirname(entryPath);
                while(directory.length != 0 && !originalFs.existsSync(directory)) {
                    directory = path.normalize(path.join(directory, ".."));
                }

                code = await testAccess(directory, originalFs.constants.W_OK);
                if(code) {
                    errors.push("Failed to acquire write permissions for directory " + entryPath + " (Code " + code.code + ". Target directory " + directory + ")");
                }
            }
        } else if(header.type == "directory") {
            let directory = path.dirname(entryPath);
            while(directory.length != 0 && !originalFs.existsSync(directory)) {
                directory = path.normalize(path.join(directory, ".."));
            }

            code = await testAccess(directory, originalFs.constants.W_OK);
            if(code) {
                errors.push("Failed to acquire write permissions for directory " + entryPath + " (Code " + code.code + ". Target directory " + directory + ")");
            }
        }
    };

    tar_stream.on('entry', (header: Headers, stream, next) => {
        tester(header).catch(error => {
            console.log("Emit out of tar_stream.on('entry' ...)");
            tar_stream.emit('error', error);
        }).then(() => {
            stream.on('end', next);
            stream.resume();
        });
    });

    fstream.pipe(tar_stream);
    try {
        await new Promise((resolve, reject) => {
            tar_stream.on('finish', resolve);
            tar_stream.on('error', error => { reject(error); });
        });
    } catch(error) {
        throw "Failed to list files within tar: " + error;
    }

    return errors;
}

namespace InstallConfig {
    export interface LockFile {
        filename: string;
        timeout: number;
        "error-id": string;
    }
    export interface MoveFile {
        source: string;
        target: string;
        "error-id": string;
    }
    export interface ConfigFile {
        version: number;

        backup: boolean;
        "backup-directory": string;

        "callback_file": string;
        "callback_argument_fail": string;
        "callback_argument_success": string;

        moves: MoveFile[];
        locks: LockFile[];
    }
}

async function createUpdateInstallConfig(sourceRoot: string, targetRoot: string) : Promise<InstallConfig.ConfigFile> {
    console.log("Building update install config for target directory: %s. Update source: %o", targetRoot, sourceRoot);
    const result: InstallConfig.ConfigFile = { } as any;

    result.version = 1;

    result.backup = true;

    {
        const data = path.parse(sourceRoot);
        result["backup-directory"] = path.join(data.dir, data.name + "_backup");
    }

    result["permission-test-directory"] = targetRoot;
    result.callback_file = app.getPath("exe");
    result.callback_argument_fail = "--no-single-instance --update-failed-new=";
    result.callback_argument_success = "--no-single-instance --update-succeed-new=";

    result.moves = [];
    result.locks = [
        {
            "error-id": "main-exe-lock",
            filename: app.getPath("exe"),
            timeout: 10 * 1000
        }
    ];

    const ignoreFileList = [
        "update-installer.exe", "update-installer"
    ];

    const dirWalker = async (relative_path: string) => {
        const sourceDirectory = path.join(sourceRoot, relative_path);
        const targetDirectory = path.join(targetRoot, relative_path);

        let files: string[];
        try {
            files = await util.promisify(ofs.readdir)(sourceDirectory);
        } catch(error) {
            console.warn("Failed to iterate over source directory \"%s\": %o", sourceDirectory, error);
            return;
        }

        for(const file of files) {
            let shouldBeExcluded = false;
            for(const ignoredFile of ignoreFileList) {
                if(ignoredFile == file) {
                    console.debug("Ignoring file to update (%s/%s)", relative_path, file);
                    shouldBeExcluded = true;
                    break;
                }
            }
            if(shouldBeExcluded) {
                continue;
            }

            const source_file = path.join(sourceDirectory, file);
            const target_file = path.join(targetDirectory, file);

            //TODO check if file content has changed else ignore?

            const info = await util.promisify(ofs.stat)(source_file);
            if(info.isDirectory()) {
                await dirWalker(path.join(relative_path, file));
            } else {
                /* TODO: ensure its a file! */
                result.moves.push({
                    "error-id": "move-file-" + result.moves.length,
                    source: source_file,
                    target: target_file
                });
            }
        }
    };

    await dirWalker(".");
    return result;
}

export async function extractUpdateFile(updateFile: string, callbackLog: UpdateLogCallback) : Promise<{ updateSourceDirectory: string, updateInstallerExecutable: string }> {
    const temporaryDirectory = path.join(app.getPath("temp"), "teaclient_update_" + Math.random().toString(36).substring(7));

    try {
        await fs.mkdirp(temporaryDirectory)
    } catch(error) {
        console.error("failed to create update source directory (%s): %o", temporaryDirectory, error);
        throw "failed to create update source directory";
    }

    callbackLog("info", "Extracting update to " + temporaryDirectory);
    console.log("Extracting update file %s to %s", updateFile, temporaryDirectory);

    let updateInstallerPath = undefined;

    const updateFileStream = fs.createReadStream(updateFile);
    const extract = tar.extract();

    extract.on('entry', (header: Headers, stream: PassThrough, callback) => {
        const extract = async (header: Headers, stream: PassThrough) => {
            const targetFile = path.join(temporaryDirectory, header.name);
            console.debug("Extracting entry %s of type %s to %s", header.name, header.type, targetFile);

            if(header.type == "directory") {
                await fs.mkdirp(targetFile);
            } else if(header.type == "file") {
                const targetPath = path.parse(targetFile);

                {
                    const directory = targetPath.dir;
                    console.debug("Testing for directory: %s", directory);
                    if(!(await util.promisify(ofs.exists)(directory)) || !(await util.promisify(ofs.stat)(directory)).isDirectory()) {
                        console.log("Creating directory %s", directory);
                        try {
                            await fs.mkdirp(directory);
                        } catch(error) {
                            console.warn("failed to create directory for file %s", header.type);
                        }
                    }

                }

                const write_stream = ofs.createWriteStream(targetFile);
                try {
                    await new Promise((resolve, reject) => {
                        stream.pipe(write_stream)
                            .on('error', reject)
                            .on('finish', resolve);
                    });

                    if(targetPath.name === "update-installer" || targetPath.name === "update-installer.exe") {
                        updateInstallerPath = targetFile;
                        callbackLog("info", "Found update installer at " + targetFile);
                    }

                    return; /* success */
                } catch(error) {
                    console.error("Failed to extract update file %s: %o", header.name, error);
                }
            } else {
                console.debug("Skipping this unknown file type");
            }
            stream.resume(); /* drain the stream */
        };

        extract(header, stream).catch(error => {
            console.log("Ignoring file %s due to an error: %o", header.name, error);
        }).then(() => {
            callback();
        });
    });


    updateFileStream.pipe(extract);
    try {
        await new Promise((resolve, reject) => {
            extract.on('finish', resolve);
            extract.on('error', reject);
        });
    } catch(error) {
        console.error("Failed to unpack update: %o", error);
        throw "update unpacking failed";
    }

    if(typeof updateInstallerPath !== "string" || !(await fs.pathExists(updateInstallerPath))) {
        throw "missing update installer executable within update package";
    }

    callbackLog("info", "Update successfully extracted");
    return { updateSourceDirectory: temporaryDirectory, updateInstallerExecutable: updateInstallerPath }
}

let cachedAppInfo: AppInfoFile;
async function initializeAppInfo() {
    let directory = app.getAppPath();
    if(!directory.endsWith(".asar")) {
        /* we're in a development version */
        cachedAppInfo = {
            version: 2,
            clientVersion: {
                major: 0,
                minor: 0,
                patch: 0,
                buildIndex: 0,
                timestamp: Date.now()
            },

            uiPackChannel: "release",
            clientChannel: "release"
        };
        return;
    }

    cachedAppInfo = validateAppInfo(await fs.readJson(path.join(directory, "..", "..", "app-info.json")));
    if(cachedAppInfo.version !== 2) {
        cachedAppInfo = undefined;
        throw "invalid app info version";
    }
}

export function clientAppInfo() : AppInfoFile {
    if(typeof cachedAppInfo !== "object") {
        throw "app info not initialized";
    }

    return cachedAppInfo;
}

export async function currentClientVersion() : Promise<Version> {
    if(processArguments.has_value(Arguments.UPDATER_LOCAL_VERSION)) {
        return parseVersion(processArguments.value(Arguments.UPDATER_LOCAL_VERSION));
    }

    const info = clientAppInfo();
    return new Version(info.clientVersion.major, info.clientVersion.minor, info.clientVersion.patch, info.clientVersion.buildIndex, info.clientVersion.timestamp);
}

let cachedUpdateConfig: UpdateConfigFile;
function updateConfigFile() : string {
    return path.join(electron.app.getPath('userData'), "update-settings.json");
}

export async function initializeAppUpdater() {
    try {
        await initializeAppInfo();
    } catch (error) {
        console.error("Failed to parse app info: %o", error);
        throw "Failed to parse app info file";
    }

    const config = updateConfigFile();
    if(await fs.pathExists(config)) {
        try {
            cachedUpdateConfig = validateUpdateConfig(await fs.readJson(config));
            if(cachedUpdateConfig.version !== 1) {
                cachedUpdateConfig = undefined;
                throw "invalid update config version";
            }
        } catch (error) {
            console.warn("Failed to parse update config file: %o. Invalidating it.", error);
            try {
                await fs.rename(config, config + "." + Date.now());
            } catch (_) {}
        }
    }

    if(!cachedUpdateConfig) {
        cachedUpdateConfig = {
            version: 1,
            selectedChannel: "release"
        }
    }
}

export function updateConfig() {
    if(typeof cachedUpdateConfig === "string") {
        throw "app updater hasn't been initialized yet";
    }
    return cachedUpdateConfig;
}

export function saveUpdateConfig() {
    const file = updateConfigFile();
    fs.writeJson(file, cachedUpdateConfig).catch(error => {
        console.error("Failed to save update config: %o", error);
    });
}

/* Attention: The current channel might not be the channel the client has initially been loaded with! */
export function clientUpdateChannel() : string {
    return updateConfig().selectedChannel;
}

export function setClientUpdateChannel(channel: string) {
    if(updateConfig().selectedChannel == channel) {
        return;
    }

    updateConfig().selectedChannel = channel;
    saveUpdateConfig();
}

export async function availableClientUpdate() : Promise<UpdateVersion | undefined> {
    const version = await newestRemoteClientVersion(clientAppInfo().clientChannel);
    if(!version) { return undefined; }

    const localVersion = await currentClientVersion();
    return !localVersion.isDevelopmentVersion() && version.version.newerThan(localVersion) ? version : undefined;
}

/**
 * The `callbackLog` might get called after this method exists and as soon you call the `callbackExecute`.
 * @returns The callback to execute the update
 */
export async function prepareUpdateExecute(targetVersion: UpdateVersion, callbackStats: UpdateStatsCallback, callbackLog: UpdateLogCallback) : Promise<{ callbackExecute: () => void, callbackAbort: () => void }> {
    let targetApplicationPath = app.getAppPath();
    if(targetApplicationPath.endsWith(".asar")) {
        console.log("App path points to ASAR file (Going up to root directory)");
        targetApplicationPath = await fs.realpath(path.join(targetApplicationPath, "..", ".."));
    } else {
        throw "the source can't be updated";
    }

    callbackStats("Downloading update", 0);
    const updateFilePath = await downloadClientVersion(targetVersion.channel, targetVersion.version, status => {
        callbackStats("Downloading update", status.percent);
    }, callbackLog);

    callbackStats("Extracting update", .5);
    const { updateSourceDirectory, updateInstallerExecutable } = await extractUpdateFile(updateFilePath, callbackLog);

    callbackStats("Generating install config", .5);

    callbackLog("info", "Generating install config");
    let installConfig;
    try {
        installConfig = await createUpdateInstallConfig(updateSourceDirectory, targetApplicationPath);
    } catch(error) {
        console.error("Failed to build update installer config: %o", error);
        throw "failed to build update installer config";
    }

    const installLogFile = path.join(updateSourceDirectory, "update-log.txt");
    const installConfigFile = path.join(updateSourceDirectory, "update_install.json");
    console.log("Writing config to %s", installConfigFile);
    try {
        await fs.writeJSON(installConfigFile, installConfig);
    } catch(error) {
        console.error("Failed to write update install config file: %s", error);
        throw "failed to write update install config file";
    }

    callbackLog("info", "Generating config generated at " + installConfigFile);

    let executeCallback: () => void;
    if(os.platform() == "linux") {
        console.log("Executing update install on linux");

        /* We must be on a unix based system */
        callbackLog("info", "Checking file permissions");
        const inaccessiblePaths = await ensureTargetFilesAreWriteable(updateFilePath);
        if(inaccessiblePaths.length > 0) {
            console.log("Failed to access the following files:");
            for(const fail of inaccessiblePaths) {
                console.log(" - " + fail);
            }
            console.log("Will prompt the user to execute a command");
        }

        try {
            fs.chmodSync(updateInstallerExecutable, 0o755);
        } catch (error) {
            console.error("Failed to make update executable executable: %o", error);
            throw "failed to make update executable executable";
        }

        //We have to unpack it later
        executeCallback = () => {
            if(inaccessiblePaths.length > 0) {
                const updateCommand = `sudo "${updateInstallerExecutable}" "${installLogFile}" "${installConfigFile}" no-daemon`;
                try {
                    electron.clipboard.writeText(updateCommand);
                } catch (error) {
                    console.error("Failed to copy command to clipboard: %o", error);
                }

                callbackLog("error",
                    "We don't have permissions to write to all files.\n" +
                    "Please do the following steps:\n" +
                    "1. Close this client\n" +
                    "2. Execute this command:\n" +
                    "   " + updateCommand + "\n" +
                    "3. Start the client again\n\n" +
                    "Note:\n" +
                    "We've already copied that command to your clipboard."
                );
                return;
            }

            console.log("Executing command %s with args %o", updateInstallerExecutable, [installLogFile, installConfigFile]);
            try {
                let result = child_process.spawnSync(updateInstallerExecutable, [installLogFile, installConfigFile]);
                if(result.status != 0) {
                    console.error("Failed to execute update installer! Return code: %d", result.status);
                    dialog.showMessageBox({
                        buttons: ["update now", "remind me later"],
                        title: "Update available",
                        message:
                            "Failed to execute update installer\n" +
                            "Installer exited with code " + result.status
                    } as MessageBoxOptions);
                    return;
                }
            } catch(error) {
                console.error("Failed to execute update installer (%o)", error);
                if("errno" in error) {
                    const errno = error as ErrnoException;
                    if(errno.errno == os.constants.errno.EPERM) {
                        dialog.showMessageBox({
                            buttons: ["quit"],
                            title: "Update execute failed",
                            message: "Failed to execute update installer. (No permissions)\nPlease execute the client with admin privileges!"
                        } as MessageBoxOptions);
                        return;
                    }
                    dialog.showMessageBox({
                        buttons: ["quit"],
                        title: "Update execute failed",
                        message: "Failed to execute update installer.\nError: " + errno.message
                    } as MessageBoxOptions);
                    return;
                }
                dialog.showMessageBox({
                    buttons: ["quit"],
                    title: "Update execute failed",
                    message: "Failed to execute update installer.\nLookup console for more detail"
                } as MessageBoxOptions);
                return;
            }

            if(electron.app.hasSingleInstanceLock()) {
                electron.app.releaseSingleInstanceLock();
            }

            const ids = child_process.execSync("pgrep TeaClient").toString().split(os.EOL).map(e => e.trim()).reverse().join(" ");
            console.log("Executing %s", "kill -9 " + ids);
            child_process.execSync("kill -9 " + ids);
        };
    } else {
        console.log("Executing update install on windows");

        executeCallback = () => {
            console.log("Executing command %s with args %o", updateInstallerExecutable, [installLogFile, installConfigFile]);

            try {
                const pipe = child_process.spawn(updateInstallerExecutable, [installLogFile, installConfigFile], {
                    detached: true,
                    shell: true,
                    cwd: path.dirname(app.getAppPath()),
                    stdio: "ignore"
                });
                pipe.unref();
                app.quit();
            } catch(error) {
                console.dir(error);
                electron.dialog.showErrorBox("Failed to finalize update", "Failed to finalize update.\nInvoking the update-installer.exe failed.\nLookup the console for more details.");
            }
        };
    }

    callbackStats("Update successfully prepared", 1);
    callbackLog("info", "Update successfully prepared");

    return {
        callbackExecute: executeCallback,
        callbackAbort: () => {
            /* TODO: Cleanup */
        }
    }
}