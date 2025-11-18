import { ConnectionHandler } from "../ConnectionHandler";
export declare enum Sound {
    SOUND_TEST = "sound.test",
    SOUND_EGG = "sound.egg",
    AWAY_ACTIVATED = "away_activated",
    AWAY_DEACTIVATED = "away_deactivated",
    MICROPHONE_MUTED = "microphone.muted",
    MICROPHONE_ACTIVATED = "microphone.activated",
    SOUND_MUTED = "sound.muted",
    SOUND_ACTIVATED = "sound.activated",
    CONNECTION_CONNECTED = "connection.connected",
    CONNECTION_DISCONNECTED = "connection.disconnected",
    CONNECTION_BANNED = "connection.banned",
    CONNECTION_DISCONNECTED_TIMEOUT = "connection.disconnected.timeout",
    CONNECTION_REFUSED = "connection.refused",
    SERVER_EDITED = "server.edited",
    SERVER_EDITED_SELF = "server.edited.self",
    SERVER_KICKED = "server.kicked",
    CHANNEL_CREATED = "channel.created",
    CHANNEL_MOVED = "channel.moved",
    CHANNEL_EDITED = "channel.edited",
    CHANNEL_EDITED_SELF = "channel.edited.self",
    CHANNEL_DELETED = "channel.deleted",
    CHANNEL_JOINED = "channel.joined",
    CHANNEL_KICKED = "channel.kicked",
    USER_MOVED = "user.moved",
    USER_MOVED_SELF = "user.moved.self",
    USER_POKED_SELF = "user.poked.self",
    USER_BANNED = "user.banned",
    USER_ENTERED = "user.joined",
    USER_ENTERED_MOVED = "user.joined.moved",
    USER_ENTERED_KICKED = "user.joined.kicked",
    USER_ENTERED_CONNECT = "user.joined.connect",
    USER_LEFT = "user.left",
    USER_LEFT_MOVED = "user.left.moved",
    USER_LEFT_KICKED_CHANNEL = "user.left.kicked.server",
    USER_LEFT_KICKED_SERVER = "user.left.kicked.channel",
    USER_LEFT_DISCONNECT = "user.left.disconnect",
    USER_LEFT_BANNED = "user.left.banned",
    USER_LEFT_TIMEOUT = "user.left.timeout",
    ERROR_INSUFFICIENT_PERMISSIONS = "error.insufficient_permissions",
    MESSAGE_SEND = "message.send",
    MESSAGE_RECEIVED = "message.received",
    GROUP_SERVER_ASSIGNED = "group.server.assigned",
    GROUP_SERVER_REVOKED = "group.server.revoked",
    GROUP_CHANNEL_CHANGED = "group.channel.changed",
    GROUP_SERVER_ASSIGNED_SELF = "group.server.assigned.self",
    GROUP_SERVER_REVOKED_SELF = "group.server.revoked.self",
    GROUP_CHANNEL_CHANGED_SELF = "group.channel.changed.self"
}
export interface SoundHandle {
    key: string;
    filename: string;
}
export interface SoundFile {
    path: string;
    volume?: number;
}
export declare function get_sound_volume(sound: Sound, default_volume?: number): number;
export declare function set_sound_volume(sound: Sound, volume: number): void;
export declare function get_master_volume(): number;
export declare function setSoundMasterVolume(volume: number): void;
export declare function overlap_activated(): boolean;
export declare function set_overlap_activated(flag: boolean): void;
export declare function ignore_output_muted(): boolean;
export declare function set_ignore_output_muted(flag: boolean): void;
export declare function save(): void;
export declare function initializeSounds(): Promise<void>;
export interface PlaybackOptions {
    ignore_muted?: boolean;
    ignore_overlap?: boolean;
    default_volume?: number;
    callback?: (flag: boolean) => any;
}
export declare function resolve_sound(sound: Sound): Promise<SoundHandle>;
export declare let manager: SoundManager;
export declare class SoundManager {
    private readonly _handle;
    private _playing_sounds;
    constructor(handle: ConnectionHandler);
    play(_sound: Sound, options?: PlaybackOptions): void;
}
export interface SoundBackend {
    playSound(sound: SoundFile): Promise<void>;
}
export declare function getSoundBackend(): SoundBackend;
export declare function setSoundBackend(newSoundBackend: SoundBackend): void;
