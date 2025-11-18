import { ConnectionHandler } from "../ConnectionHandler";
import { Registry } from "../events";
import { VideoBroadcastType } from "tc-shared/connection/VideoConnection";
import { PermissionEditorTab } from "tc-shared/ui/modal/permission/ModalDefinitions";
export interface ClientGlobalControlEvents {
    action_open_window: {
        window: "settings" | /* use action_open_window_settings! */ "about" | "settings-registry" | "css-variable-editor" | "bookmark-manage" | "query-manage" | "query-create" | "ban-list" | "permissions" | "token-list" | "token-use" | "server-echo-test";
        connection?: ConnectionHandler;
    };
    action_w2g: {
        following: number;
        handlerId: string;
    } | {
        videoUrl: string;
        handlerId: string;
    };
    action_toggle_video_broadcasting: {
        connection: ConnectionHandler;
        broadcastType: VideoBroadcastType;
        enabled: boolean;
        quickSelect?: boolean;
        defaultDevice?: string;
    };
    action_edit_video_broadcasting: {
        connection: ConnectionHandler;
        broadcastType: VideoBroadcastType;
    };
    action_open_window_connect: {
        newTab: boolean;
    };
    action_open_window_settings: {
        defaultCategory?: string;
    };
    action_open_window_permissions: {
        connection?: ConnectionHandler;
        defaultTab: PermissionEditorTab;
    };
}
export declare const global_client_actions: Registry<ClientGlobalControlEvents>;
