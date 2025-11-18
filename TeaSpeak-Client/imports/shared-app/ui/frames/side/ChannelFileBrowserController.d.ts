import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { Registry } from "tc-shared/events";
import { ChannelFileBrowserUiEvents } from "tc-shared/ui/frames/side/ChannelFileBrowserDefinitions";
import { ChannelEntry } from "tc-shared/tree/Channel";
export declare class ChannelFileBrowserController {
    readonly uiEvents: Registry<ChannelFileBrowserUiEvents>;
    private currentConnection;
    private remoteBrowseEvents;
    private currentChannel;
    constructor();
    destroy(): void;
    setConnectionHandler(connection: ConnectionHandler): void;
    setChannel(channel: ChannelEntry | undefined): void;
    private notifyEvents;
}
