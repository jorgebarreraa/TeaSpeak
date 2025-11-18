import { Registry } from "tc-shared/events";
export declare const kIPCIconChannel = "icons";
export declare const kGlobalIconHandlerId = "global";
export interface RemoteIconEvents {
    notify_state_changed: {
        oldState: RemoteIconState;
        newState: RemoteIconState;
    };
}
export declare type RemoteIconState = "loading" | "loaded" | "error" | "empty" | "destroyed";
export declare type RemoteIconInfo = {
    iconId: number;
    serverUniqueId: string;
    handlerId?: string;
};
export declare abstract class RemoteIcon {
    readonly events: Registry<RemoteIconEvents>;
    readonly iconId: number;
    readonly serverUniqueId: string;
    private state;
    protected imageUrl: string;
    protected errorMessage: string;
    protected constructor(serverUniqueId: string, iconId: number);
    destroy(): void;
    getState(): RemoteIconState;
    protected setState(state: RemoteIconState): void;
    hasImageUrl(): boolean;
    /**
     * Will throw an string if the icon isn't in loaded state
     */
    getImageUrl(): string;
    protected setImageUrl(url: string): void;
    /**
     * Will throw an string if the state isn't error
     */
    getErrorMessage(): string | undefined;
    protected setErrorMessage(message: string): void;
    /**
     * Waits 'till the icon has been loaded or any other, non loading, state has been reached.
     */
    awaitLoaded(): Promise<void>;
    /**
     * Returns true if the icon isn't loading any more.
     * This includes all other states like error, destroy or empty.
     */
    isLoaded(): boolean;
}
export declare abstract class AbstractIconManager {
    protected static iconUniqueKey(iconId: number, serverUniqueId: string): string;
    resolveIconInfo(icon: RemoteIconInfo): RemoteIcon;
    /**
     * @param iconId The requested icon
     * @param serverUniqueId The server unique id for the icon
     * @param handlerId Hint which connection handler should be used if we're downloading the icon
     */
    abstract resolveIcon(iconId: number, serverUniqueId?: string, handlerId?: string): RemoteIcon;
}
export declare function setIconManager(instance: AbstractIconManager): void;
export declare function getIconManager(): AbstractIconManager;
export declare function generateIconJQueryTag(icon: RemoteIcon | undefined, options?: {
    animate?: boolean;
}): JQuery<HTMLDivElement>;
