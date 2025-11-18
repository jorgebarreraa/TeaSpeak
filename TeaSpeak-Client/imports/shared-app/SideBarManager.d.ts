import { SideBarType } from "tc-shared/ui/frames/SideBarDefinitions";
import { Registry } from "tc-shared/events";
import { ClientEntry, MusicClientEntry } from "tc-shared/tree/Client";
import { ConnectionHandler } from "tc-shared/ConnectionHandler";
export interface SideBarManagerEvents {
    notify_content_type_changed: {
        newContent: SideBarType;
    };
}
export declare class SideBarManager {
    readonly events: Registry<SideBarManagerEvents>;
    private readonly connection;
    private currentType;
    constructor(connection: ConnectionHandler);
    destroy(): void;
    getSideBarContent(): SideBarType;
    setSideBarContent(content: SideBarType): void;
    showPrivateConversations(): void;
    showChannel(): void;
    showServer(): void;
    showClientInfo(client: ClientEntry): void;
    showMusicPlayer(_client: MusicClientEntry): void;
    clearSideBar(): void;
}
