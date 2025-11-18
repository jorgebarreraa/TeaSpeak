export interface TranslationKey {
    message: string;
    line?: number;
    character?: number;
    filename?: string;
}
export interface Translation {
    key: TranslationKey;
    translated: string;
    flags?: string[];
}
export interface Contributor {
    name: string;
    email: string;
}
export interface TranslationFile {
    path: string;
    full_url: string;
    translations: Translation[];
}
export interface RepositoryTranslation {
    key: string;
    path: string;
    country_code: string;
    name: string;
    contributors: Contributor[];
}
export interface TranslationRepository {
    unique_id: string;
    url: string;
    name?: string;
    contact?: string;
    translations?: RepositoryTranslation[];
    load_timestamp?: number;
}
export declare function tr(message: string, key?: string): string;
export declare function trJQuery(message: string, ...args: any[]): JQuery[];
export declare function tra(message: string, ...args: (string | number | boolean)[]): string;
export declare function load_file(url: string, path: string): Promise<void>;
export declare function load_repository(url: string): Promise<TranslationRepository>;
export declare namespace config {
    interface TranslationConfig {
        current_repository_url?: string;
        current_language?: string;
        current_translation_url?: string;
        current_translation_path?: string;
    }
    interface RepositoryConfig {
        repositories?: {
            url?: string;
            repository?: TranslationRepository;
        }[];
    }
    function repository_config(): RepositoryConfig;
    function save_repository_config(): void;
    function translation_config(): TranslationConfig;
    function save_translation_config(): void;
}
export declare function register_repository(repository: TranslationRepository): void;
export declare function registered_repositories(): TranslationRepository[];
export declare function delete_repository(repository: TranslationRepository): void;
export declare function iterate_repositories(callback_entry: (repository: TranslationRepository) => any): Promise<void>;
export declare function select_translation(repository: TranslationRepository, entry: RepositoryTranslation): void;
export declare function initializeI18N(): Promise<void>;
declare global {
    interface Window {
        tr(message: string): string;
        log: any;
        StaticSettings: any;
    }
    const tr: typeof window.tr;
}
