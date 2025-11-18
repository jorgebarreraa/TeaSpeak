import * as contextmenu from "../ui/elements/ContextMenu";
import { Registry } from "../events";
import { ChannelTree } from "./ChannelTree";
import { Group } from "../permission/GroupManager";
import { ChannelEntry } from "./Channel";
import { ConnectionHandler } from "../ConnectionHandler";
import { ChannelTreeEntry, ChannelTreeEntryEvents } from "./ChannelTreeEntry";
import { ClientIcon } from "svg-sprites/client-icons";
import { VoiceClient } from "../voice/VoiceClient";
import { ChannelTreeUIEvents } from "tc-shared/ui/tree/Definitions";
import { VideoClient } from "tc-shared/connection/VideoConnection";
import { EventClient } from "tc-shared/connectionlog/Definitions";
export declare enum ClientType {
    CLIENT_VOICE = 0,
    CLIENT_QUERY = 1,
    CLIENT_WEB = 3,
    CLIENT_MUSIC = 4,
    CLIENT_TEASPEAK = 5,
    CLIENT_UNDEFINED = 5
}
export declare class ClientProperties {
    client_type: ClientType;
    client_type_exact: ClientType;
    client_database_id: number;
    client_version: string;
    client_platform: string;
    client_nickname: string;
    client_unique_identifier: string;
    client_description: string;
    client_servergroups: string;
    client_channel_group_id: number;
    client_channel_group_inherited_channel_id: number;
    client_lastconnected: number;
    client_created: number;
    client_totalconnections: number;
    client_flag_avatar: string;
    client_icon_id: number;
    client_away_message: string;
    client_away: boolean;
    client_country: string;
    client_input_hardware: boolean;
    client_output_hardware: boolean;
    client_input_muted: boolean;
    client_output_muted: boolean;
    client_is_channel_commander: boolean;
    client_teaforo_id: number;
    client_teaforo_name: string;
    client_teaforo_flags: number;
    client_month_bytes_uploaded: number;
    client_month_bytes_downloaded: number;
    client_total_bytes_uploaded: number;
    client_total_bytes_downloaded: number;
    client_talk_power: number;
    client_talk_request: number;
    client_talk_request_msg: string;
    client_is_talker: boolean;
    client_is_priority_speaker: boolean;
}
export declare class ClientConnectionInfo {
    connection_bandwidth_received_last_minute_control: number;
    connection_bandwidth_received_last_minute_keepalive: number;
    connection_bandwidth_received_last_minute_speech: number;
    connection_bandwidth_received_last_second_control: number;
    connection_bandwidth_received_last_second_keepalive: number;
    connection_bandwidth_received_last_second_speech: number;
    connection_bandwidth_sent_last_minute_control: number;
    connection_bandwidth_sent_last_minute_keepalive: number;
    connection_bandwidth_sent_last_minute_speech: number;
    connection_bandwidth_sent_last_second_control: number;
    connection_bandwidth_sent_last_second_keepalive: number;
    connection_bandwidth_sent_last_second_speech: number;
    connection_bytes_received_control: number;
    connection_bytes_received_keepalive: number;
    connection_bytes_received_speech: number;
    connection_bytes_sent_control: number;
    connection_bytes_sent_keepalive: number;
    connection_bytes_sent_speech: number;
    connection_packets_received_control: number;
    connection_packets_received_keepalive: number;
    connection_packets_received_speech: number;
    connection_packets_sent_control: number;
    connection_packets_sent_keepalive: number;
    connection_packets_sent_speech: number;
    connection_ping: number;
    connection_ping_deviation: number;
    connection_server2client_packetloss_control: number;
    connection_server2client_packetloss_keepalive: number;
    connection_server2client_packetloss_speech: number;
    connection_server2client_packetloss_total: number;
    connection_client2server_packetloss_speech: number;
    connection_client2server_packetloss_keepalive: number;
    connection_client2server_packetloss_control: number;
    connection_client2server_packetloss_total: number;
    connection_filetransfer_bandwidth_sent: number;
    connection_filetransfer_bandwidth_received: number;
    connection_connected_time: number;
    connection_idle_time: number;
    connection_client_ip: string | undefined;
    connection_client_port: number;
}
export interface ClientEvents extends ChannelTreeEntryEvents {
    notify_properties_updated: {
        updated_properties: Partial<ClientProperties>;
        client_properties: ClientProperties;
    };
    notify_mute_state_change: {
        muted: boolean;
    };
    notify_speak_state_change: {
        speaking: boolean;
    };
    notify_audio_level_changed: {
        newValue: number;
    };
    notify_status_icon_changed: {
        newIcon: ClientIcon;
    };
    notify_video_handle_changed: {
        oldHandle: VideoClient | undefined;
        newHandle: VideoClient | undefined;
    };
}
export declare class ClientEntry<Events extends ClientEvents = ClientEvents> extends ChannelTreeEntry<Events> {
    readonly events: Registry<Events>;
    channelTree: ChannelTree;
    protected _clientId: number;
    protected _channel: ChannelEntry;
    protected _properties: ClientProperties;
    protected lastVariableUpdate: number;
    protected _speaking: boolean;
    protected voiceHandle: VoiceClient;
    protected voiceVolume: number;
    protected voiceMuted: boolean;
    private readonly voiceCallbackStateChanged;
    protected videoHandle: VideoClient;
    private promiseClientInfo;
    private promiseClientInfoTimestamp;
    private promiseConnectionInfo;
    private promiseConnectionInfoTimestamp;
    private promiseConnectionInfoResolve;
    private promiseConnectionInfoReject;
    constructor(clientId: number, clientName: any, properties?: ClientProperties);
    destroy(): void;
    setVoiceClient(handle: VoiceClient): void;
    setVideoClient(handle: VideoClient): void;
    private handleVoiceStateChange;
    private updateVoiceVolume;
    getVoiceClient(): VoiceClient;
    getVideoClient(): VideoClient;
    get properties(): ClientProperties;
    getStatusIcon(): ClientIcon;
    currentChannel(): ChannelEntry;
    clientNickName(): string;
    clientUid(): string;
    clientId(): number;
    isMuted(): boolean;
    setMuted(flagMuted: boolean, force: boolean): void;
    protected contextmenu_info(): contextmenu.MenuEntry[];
    protected assignment_context(): contextmenu.MenuEntry[];
    open_assignment_modal(): void;
    open_text_chat(): void;
    showContextMenu(x: number, y: number, on_close?: () => void): void;
    static bbcodeTag(id: number, name: string, uid: string): string;
    static chatTag(id: number, name: string, uid: string, braces?: boolean): JQuery;
    create_bbcode(): string;
    createChatTag(braces?: boolean): JQuery;
    /** @deprecated Don't use this any more! */
    set speaking(flag: boolean);
    isSpeaking(): boolean;
    protected setSpeaking(flag: boolean): void;
    updateVariables(...variables: {
        key: string;
        value: string;
    }[]): void;
    updateClientVariables(force_update?: boolean): Promise<void>;
    assignedServerGroupIds(): number[];
    assignedChannelGroup(): number;
    groupAssigned(group: Group): boolean;
    onDelete(): void;
    calculateOnlineTime(): number;
    avatarId?(): string;
    log_data(): EventClient;
    request_connection_info(): Promise<ClientConnectionInfo>;
    set_connection_info(info: ClientConnectionInfo): void;
    setAudioVolume(value: number): void;
    getAudioVolume(): number;
    getClientType(): ClientType;
}
export declare class LocalClientEntry extends ClientEntry {
    handle: ConnectionHandler;
    constructor(handle: ConnectionHandler);
    setSpeaking(flag: boolean): void;
    showContextMenu(x: number, y: number, on_close?: () => void): void;
    renameSelf(new_name: string): Promise<boolean>;
    openRenameModal(): void;
    openRename(events: Registry<ChannelTreeUIEvents>): void;
}
export declare enum MusicClientPlayerState {
    SLEEPING = 0,
    LOADING = 1,
    PLAYING = 2,
    PAUSED = 3,
    STOPPED = 4
}
export declare class MusicClientProperties extends ClientProperties {
    player_state: number;
    player_volume: number;
    client_playlist_id: number;
    client_disabled: boolean;
    client_flag_notify_song_change: boolean;
    client_bot_type: number;
    client_uptime_mode: number;
}
export declare class SongInfo {
    song_id: number;
    song_url: string;
    song_invoker: number;
    song_loaded: boolean;
    song_title: string;
    song_description: string;
    song_thumbnail: string;
    song_length: number;
}
export declare class MusicClientPlayerInfo extends SongInfo {
    bot_id: number;
    player_state: number;
    player_buffered_index: number;
    player_replay_index: number;
    player_max_index: number;
    player_seekable: boolean;
    player_title: string;
    player_description: string;
}
export interface MusicClientEvents extends ClientEvents {
    notify_music_player_song_change: {
        newSong: SongInfo | undefined;
    };
    notify_music_player_timestamp: {
        bufferedIndex: number;
        replayIndex: number;
    };
    notify_subscribe_state_changed: {
        subscribed: boolean;
    };
}
export declare class MusicClientEntry extends ClientEntry<MusicClientEvents> {
    private subscribed;
    private _info_promise;
    private _info_promise_age;
    private _info_promise_resolve;
    private _info_promise_reject;
    constructor(clientId: any, clientName: any);
    destroy(): void;
    get properties(): MusicClientProperties;
    isSubscribed(): boolean;
    subscribe(): Promise<void>;
    showContextMenu(x: number, y: number, on_close?: () => void): void;
    handlePlayerInfo(json: any): void;
    requestPlayerInfo(max_age?: number): Promise<MusicClientPlayerInfo>;
    isCurrentlyPlaying(): boolean;
}
