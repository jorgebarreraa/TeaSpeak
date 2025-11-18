/// <reference types="react" />
import { Registry } from "tc-shared/events";
import { MusicPlaylistUiEvents } from "tc-shared/ui/frames/side/MusicPlaylistDefinitions";
import { MusicBotUiEvents } from "tc-shared/ui/frames/side/MusicBotDefinitions";
export declare const MusicBotRenderer: (props: {
    botEvents: Registry<MusicBotUiEvents>;
    playlistEvents: Registry<MusicPlaylistUiEvents>;
}) => JSX.Element;
