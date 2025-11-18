import { ServerCommand } from "../connection/ConnectionBase";
import { ClientNameInfo, Playlist, PlaylistInfo, PlaylistSong, QueryList, ServerGroupClient } from "../connection/ServerConnectionDeclaration";
import { AbstractCommandHandler } from "../connection/AbstractCommandHandler";
export declare class CommandHelper extends AbstractCommandHandler {
    private whoAmIResponse;
    private infoByUniqueIdRequest;
    private infoByDatabaseIdRequest;
    constructor(connection: any);
    initialize(): void;
    destroy(): void;
    handle_command(command: ServerCommand): boolean;
    getInfoFromUniqueId(...uniqueIds: string[]): Promise<ClientNameInfo[]>;
    private handleNotifyClientGetNameFromDatabaseId;
    getInfoFromClientDatabaseId(...clientDatabaseIds: number[]): Promise<ClientNameInfo[]>;
    private handleNotifyClientNameFromUniqueId;
    requestQueryList(server_id?: number): Promise<QueryList>;
    requestPlaylistList(): Promise<Playlist[]>;
    requestPlaylistSongs(playlist_id: number, process_result?: boolean): Promise<PlaylistSong[]>;
    request_playlist_client_list(playlist_id: number): Promise<number[]>;
    requestClientsByServerGroup(group_id: number): Promise<ServerGroupClient[]>;
    requestPlaylistInfo(playlist_id: number): Promise<PlaylistInfo>;
    /**
     * @deprecated
     *  Its just a workaround for the query management.
     *  There is no garantee that the whoami trick will work forever
     */
    getCurrentVirtualServerId(): Promise<number>;
}
