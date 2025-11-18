import { ConnectionHandler } from "tc-shared/ConnectionHandler";
export declare class ClientInfoController {
    private readonly uiEvents;
    private connection;
    private listenerConnection;
    private listenerInheritedChannel;
    private inheritedChannelInfo;
    constructor();
    destroy(): void;
    setConnectionHandler(connection: ConnectionHandler): void;
    private initializeConnection;
    private updateInheritedInfo;
    private generateGroupInfo;
    private sendClient;
    private sendChannelGroup;
    private sendServerGroups;
    private sendClientStatus;
    private sendClientName;
    private sendClientDescription;
    private sendOnline;
    private sendCountry;
    private sendVolume;
    private sendVersion;
    private sendForum;
}
