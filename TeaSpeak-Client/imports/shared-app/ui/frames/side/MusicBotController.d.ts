import { Registry } from "tc-shared/events";
import { MusicBotUiEvents } from "tc-shared/ui/frames/side/MusicBotDefinitions";
import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { MusicClientEntry } from "tc-shared/tree/Client";
import { MusicPlaylistUiEvents } from "tc-shared/ui/frames/side/MusicPlaylistDefinitions";
export declare class MusicBotController {
    private readonly uiEvents;
    private readonly playlistController;
    private listenerConnection;
    private listenerBot;
    private currentConnection;
    private currentBot;
    private playerTimestamp;
    private currentSongInfo;
    constructor();
    destroy(): void;
    getBotUiEvents(): Registry<MusicBotUiEvents>;
    getPlaylistUiEvents(): Registry<MusicPlaylistUiEvents>;
    setConnection(connection: ConnectionHandler): void;
    setBot(bot: MusicClientEntry): void;
    private initializeConnectionListener;
    private initializeBotListener;
    private updatePlaylist;
    private updatePlayerInfo;
    private reportPlayerState;
    private reportSongInfo;
    private reportPlayerTimestamp;
    private reportVolume;
}
