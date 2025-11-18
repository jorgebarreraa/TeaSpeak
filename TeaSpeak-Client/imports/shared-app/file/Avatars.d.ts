import { Registry } from "../events";
export declare const kIPCAvatarChannel = "avatars";
export declare const kLoadingAvatarImage = "img/loading_image.svg";
export declare const kDefaultAvatarImage = "img/style/avatar.png";
export declare type AvatarState = "unset" | "loading" | "errored" | "loaded";
export interface AvatarStateData {
    "unset": {};
    "loading": {};
    "errored": {
        message: string;
    };
    "loaded": {
        url: string;
    };
}
interface AvatarEvents {
    avatar_changed: {
        newAvatarHash: string;
    };
    avatar_state_changed: {
        oldState: AvatarState;
        newState: AvatarState;
        newStateData: AvatarStateData[keyof AvatarStateData];
    };
}
export declare abstract class ClientAvatar {
    readonly events: Registry<AvatarEvents>;
    readonly clientAvatarId: string;
    private currentAvatarHash;
    private state;
    private stateData;
    loadingTimestamp: number;
    constructor(clientAvatarId: string);
    destroy(): void;
    protected setState<T extends AvatarState>(state: T, data: AvatarStateData[T], force?: boolean): void;
    getTypedStateData<T extends AvatarState>(state: T): AvatarStateData[T];
    setUnset(): void;
    setLoading(): void;
    setLoaded(data: AvatarStateData["loaded"]): void;
    setErrored(data: AvatarStateData["errored"]): void;
    awaitLoaded(): Promise<true>;
    awaitLoaded(timeout: number): Promise<boolean>;
    getState(): AvatarState;
    getStateData(): AvatarStateData[AvatarState];
    getAvatarHash(): string | "unknown";
    getAvatarUrl(): string;
    getLoadError(): string;
    protected abstract destroyStateData(state: AvatarState, data: AvatarStateData[AvatarState]): any;
}
export declare abstract class AbstractAvatarManager {
    abstract resolveAvatar(clientAvatarId: string, avatarHash?: string): ClientAvatar;
    abstract resolveClientAvatar(client: {
        id?: number;
        database_id?: number;
        clientUniqueId: string;
    }): any;
}
export declare abstract class AbstractAvatarManagerFactory {
    abstract hasManager(handlerId: string): boolean;
    abstract getManager(handlerId: string): AbstractAvatarManager;
}
export declare function setGlobalAvatarManagerFactory(factory: AbstractAvatarManagerFactory): void;
export declare function getGlobalAvatarManagerFactory(): AbstractAvatarManagerFactory;
export declare function uniqueId2AvatarId(unique_id: string): string;
export {};
