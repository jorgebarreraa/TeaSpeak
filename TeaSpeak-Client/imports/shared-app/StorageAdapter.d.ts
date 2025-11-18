/**
 * Application storage meant for small and medium large internal app states.
 * Possible data would be non user editable cached values like auth tokens.
 * Note:
 * 1. Please consider using a Settings key first before using the storage adapter!
 * 2. The values may not be synced across multiple window instances.
 *    Don't use this for IPC.
 */
export interface StorageAdapter {
    has(key: string): Promise<boolean>;
    get(key: string): Promise<string | null>;
    set(key: string, value: string): Promise<void>;
    delete(key: string): Promise<void>;
}
export declare function getStorageAdapter(): StorageAdapter;
export declare function setStorageAdapter(adapter: StorageAdapter): void;
