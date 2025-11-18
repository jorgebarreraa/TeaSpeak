import * as electron from "electron";
import * as path from "path";
import * as fs from "fs-extra";
import {StorageAdapter} from "tc-shared/StorageAdapter";
import {ServerSettingsStorage} from "tc-shared/ServerSettings";
import {LogCategory, logError} from "tc-shared/log";

const kStoragePath = path.join(electron.remote.app.getPath("userData"), "settings");

function storageKeyPath(key: string) {
    return path.join(kStoragePath, encodeURIComponent(key));
}

export class ClientStorageAdapter implements StorageAdapter {
    delete(key: string): Promise<void> {
        return fs.remove(storageKeyPath(key)).catch(error => {});
    }

    async get(key: string): Promise<string | null> {
        try {
            const result = await fs.readFile(storageKeyPath(key));
            return JSON.parse(result.toString());
        } catch (error) {
            if(error.code === "ENOENT") {
                /* The file does not exists */
                return null;
            }

            logError(LogCategory.GENERAL, tr("Failed to load client storage key %s: %o"), key, error);
            return null;
        }
    }

    has(key: string): Promise<boolean> {
        return fs.pathExists(storageKeyPath(key));
    }

    set(key: string, value: string): Promise<void> {
        return fs.writeFile(storageKeyPath(key), JSON.stringify(value)).catch(error => {
            logError(LogCategory.GENERAL, tr("Failed to set client storage key %s: %o"), key, error);
        });
    }
}
export let clientStorage;

export class ClientServerSettingsStorage implements ServerSettingsStorage {
    get(serverUniqueId: string): string {
        try {
            const result = fs.readFileSync(storageKeyPath("settings.server_" + serverUniqueId));
            return JSON.parse(result.toString());
        } catch (error) {
            logError(LogCategory.GENERAL, tr("Failed to load individual server settings for %s: %o"), serverUniqueId, error);
            return null;
        }
    }

    set(serverUniqueId: string, value: string) {
        try {
            fs.writeFileSync(storageKeyPath("settings.server_" + serverUniqueId), JSON.stringify(value));
        } catch (error) {
            logError(LogCategory.GENERAL, tr("Failed to write individual server settings for %s: %o"), serverUniqueId, error);
            return null;
        }
    }
}

export async function initializeClientStorage() {
    await fs.mkdirp(kStoragePath);
    clientStorage = new ClientStorageAdapter();
}