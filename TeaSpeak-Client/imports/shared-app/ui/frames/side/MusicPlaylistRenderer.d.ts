/// <reference types="react" />
import { Registry } from "tc-shared/events";
import { MusicPlaylistUiEvents } from "tc-shared/ui/frames/side/MusicPlaylistDefinitions";
export declare function formatPlaytime(value: number): string;
export declare const DefaultThumbnail: (_props: {
    type: "loading" | "none-present";
}) => JSX.Element;
export declare const MusicPlaylistList: (props: {
    events: Registry<MusicPlaylistUiEvents>;
    className?: string;
}) => JSX.Element;
