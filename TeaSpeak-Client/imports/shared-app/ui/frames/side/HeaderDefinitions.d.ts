import { RemoteIconInfo } from "tc-shared/file/Icons";
export declare type SideHeaderState = SideHeaderStateNone | SideHeaderStateConversation | SideHeaderStateClient | SideHeaderStateMusicBot;
export declare type SideHeaderStateNone = {
    state: "none";
};
export declare type SideHeaderStateConversation = {
    state: "conversation";
    mode: "channel" | "private" | "server";
};
export declare type SideHeaderStateClient = {
    state: "client";
};
export declare type SideHeaderStateMusicBot = {
    state: "music-bot";
};
export declare type SideHeaderChannelState = {
    state: "not-connected";
} | {
    state: "connected";
    channelName: string;
    channelIcon: RemoteIconInfo;
    channelUserCount: number;
    channelMaxUser: number | -1;
};
export declare type SideHeaderPingInfo = {
    native: number;
    javaScript: number | undefined;
};
export declare type PrivateConversationInfo = {
    unread: number;
    open: number;
};
export declare type SideHeaderServerInfo = {
    name: string;
    icon: RemoteIconInfo;
};
export interface SideHeaderEvents {
    action_bot_manage: {};
    action_bot_add_song: {};
    action_switch_channel_chat: {};
    action_open_conversation: {};
    query_server_info: {};
    query_current_channel_state: {
        mode: "voice" | "text";
    };
    query_private_conversations: {};
    query_client_info_own_client: {};
    query_ping: {};
    notify_current_channel_state: {
        mode: "voice" | "text";
        state: SideHeaderChannelState;
    };
    notify_ping: {
        ping: SideHeaderPingInfo | undefined;
    };
    notify_private_conversations: {
        info: PrivateConversationInfo;
    };
    notify_client_info_own_client: {
        isOwnClient: boolean;
    };
    notify_server_info: {
        info: SideHeaderServerInfo | undefined;
    };
}
