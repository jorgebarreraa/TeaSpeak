import { ClientEntry, MusicClientEntry } from "tc-shared/tree/Client";
export interface VolumeChangeEvents {
    "change-volume": {
        newValue: number;
        origin: "user-input" | "reset" | "unknown";
    };
    "query-volume": {};
    "query-volume-response": {
        volume: number;
    };
    "apply-volume": {
        newValue: number;
        origin: "user-input" | "reset" | "unknown";
    };
    "apply-volume-result": {
        newValue: number;
        success: boolean;
    };
    "close-modal": {};
}
export declare function spawnClientVolumeChange(client: ClientEntry): import("tc-shared/ui/react-elements/modal/Definitions").ModalController;
export declare function spawnMusicBotVolumeChange(client: MusicClientEntry, maxValue: number): import("tc-shared/ui/react-elements/modal/Definitions").ModalController;
