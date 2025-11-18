import { Modal } from "tc-shared/ui/elements/Modal";
import { Registry } from "tc-shared/events";
declare type ProfileInfoEvent = {
    id: string;
    name: string;
    nickname: string;
    identity_type: "teaforo" | "teamspeak" | "nickname";
    identity_forum?: {
        valid: boolean;
        fallback_name: string;
    };
    identity_nickname?: {
        name: string;
        fallback_name: string;
    };
    identity_teamspeak?: {
        unique_id: string;
        fallback_name: string;
    };
};
export interface SettingProfileEvents {
    "reload-profile": {
        profile_id?: string;
    };
    "select-profile": {
        profile_id: string;
    };
    "query-profile-list": {};
    "query-profile-list-result": {
        status: "error" | "success" | "timeout";
        error?: string;
        profiles?: ProfileInfoEvent[];
    };
    "query-profile": {
        profile_id: string;
    };
    "query-profile-result": {
        status: "error" | "success" | "timeout";
        profile_id: string;
        error?: string;
        info?: ProfileInfoEvent;
    };
    "select-identity-type": {
        profile_id: string;
        identity_type: "teamspeak" | "teaforo" | "nickname" | "unset";
    };
    "query-profile-validity": {
        profile_id: string;
    };
    "query-profile-validity-result": {
        profile_id: string;
        status: "error" | "success" | "timeout";
        error?: string;
        valid?: boolean;
    };
    "create-profile": {
        name: string;
    };
    "create-profile-result": {
        status: "error" | "success" | "timeout";
        name: string;
        profile_id?: string;
        error?: string;
    };
    "delete-profile": {
        profile_id: string;
    };
    "delete-profile-result": {
        status: "error" | "success" | "timeout";
        profile_id: string;
        error?: string;
    };
    "set-default-profile": {
        profile_id: string;
    };
    "set-default-profile-result": {
        status: "error" | "success" | "timeout";
        old_profile_id: string;
        new_profile_id?: string;
        error?: string;
    };
    "set-profile-name": {
        profile_id: string;
        name: string;
    };
    "set-profile-name-result": {
        status: "error" | "success" | "timeout";
        profile_id: string;
        name?: string;
    };
    "set-default-name": {
        profile_id: string;
        name: string | null;
    };
    "set-default-name-result": {
        status: "error" | "success" | "timeout";
        profile_id: string;
        name?: string | null;
    };
    "query-identity-teamspeak": {
        profile_id: string;
    };
    "query-identity-teamspeak-result": {
        status: "error" | "success" | "timeout";
        profile_id: string;
        error?: string;
        level?: number;
    };
    "set-identity-name-name": {
        profile_id: string;
        name: string;
    };
    "set-identity-name-name-result": {
        status: "error" | "success" | "timeout";
        profile_id: string;
        error?: string;
        name?: string;
    };
    "generate-identity-teamspeak": {
        profile_id: string;
    };
    "generate-identity-teamspeak-result": {
        profile_id: string;
        status: "error" | "success" | "timeout";
        error?: string;
        level?: number;
        unique_id?: string;
    };
    "improve-identity-teamspeak-level": {
        profile_id: string;
    };
    "improve-identity-teamspeak-level-update": {
        profile_id: string;
        new_level: number;
    };
    "import-identity-teamspeak": {
        profile_id: string;
    };
    "import-identity-teamspeak-result": {
        profile_id: string;
        level?: number;
        unique_id?: string;
    };
    "export-identity-teamspeak": {
        profile_id: string;
        filename: string;
    };
    "setup-forum-connection": {};
}
export declare function spawnSettingsModal(default_page?: string): Modal;
export declare namespace modal_settings {
    interface ProfileViewSettings {
        forum_setuppable: boolean;
    }
    function initialize_identity_profiles_controller(event_registry: Registry<SettingProfileEvents>): void;
    function initialize_identity_profiles_view(container: JQuery, event_registry: Registry<SettingProfileEvents>, settings: ProfileViewSettings): void;
}
export {};
