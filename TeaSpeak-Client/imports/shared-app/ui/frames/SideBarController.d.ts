import { ConnectionHandler } from "../../ConnectionHandler";
import { SideHeaderController } from "tc-shared/ui/frames/side/HeaderController";
import { SideBarEvents } from "tc-shared/ui/frames/SideBarDefinitions";
import { Registry } from "tc-shared/events";
import { MusicBotController } from "tc-shared/ui/frames/side/MusicBotController";
export declare class SideBarController {
    readonly uiEvents: Registry<SideBarEvents>;
    private currentConnection;
    private listenerConnection;
    private header;
    private clientInfo;
    private privateConversations;
    private channelBar;
    private musicPanel;
    constructor();
    setConnection(connection: ConnectionHandler): void;
    destroy(): void;
    renderInto(container: HTMLDivElement): void;
    getMusicController(): MusicBotController;
    getHeaderController(): SideHeaderController;
    private sendContent;
    private sendContentData;
}
