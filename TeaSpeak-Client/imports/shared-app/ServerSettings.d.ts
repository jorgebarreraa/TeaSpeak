import { RegistryKey, RegistryValueType, ValuedRegistryKey } from "tc-shared/settings";
export declare class ServerSettings {
    private cacheServer;
    private settingsDestroyed;
    private serverUniqueId;
    private serverSaveWorker;
    private serverSettingsUpdated;
    constructor();
    destroy(): void;
    getValue<V extends RegistryValueType, DV>(key: RegistryKey<V>, defaultValue: DV): V | DV;
    getValue<V extends RegistryValueType>(key: ValuedRegistryKey<V>, defaultValue?: V): V;
    setValue<T extends RegistryValueType>(key: RegistryKey<T>, value?: T): void;
    setServerUniqueId(serverUniqueId: string): void;
    save(): void;
}
export interface ServerSettingsStorage {
    get(serverUniqueId: string): string;
    set(serverUniqueId: string, value: string): any;
}
export declare function setServerSettingsStorage(storage: ServerSettingsStorage): void;
