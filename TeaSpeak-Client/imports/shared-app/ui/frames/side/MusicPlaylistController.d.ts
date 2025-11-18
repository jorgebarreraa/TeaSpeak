import { SubscribedPlaylist } from "tc-shared/music/PlaylistManager";
import { Registry } from "tc-shared/events";
import { MusicPlaylistUiEvents } from "tc-shared/ui/frames/side/MusicPlaylistDefinitions";
export declare class MusicPlaylistController {
    readonly uiEvents: Registry<MusicPlaylistUiEvents>;
    private currentPlaylist;
    private listenerPlaylist;
    private currentSongId;
    constructor();
    destroy(): void;
    setCurrentPlaylist(playlist: SubscribedPlaylist | "loading"): void;
    getCurrentPlaylist(): SubscribedPlaylist | "loading" | undefined;
    getCurrentSongId(): number;
    setCurrentSongId(id: number | 0): void;
    private initializePlaylistListener;
    private reportPlaylistStatus;
    private reportPlaylistEntry;
}
