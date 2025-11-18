import { ConnectionHandler } from "tc-shared/ConnectionHandler";
export declare type GroupInfo = {
    id: number;
    name: string;
    type: "query" | "template" | "normal";
};
export interface GroupPermissionCopyModalEvents {
    action_set_source: {
        group: number;
    };
    action_set_target: {
        group: number;
    };
    action_cancel: {};
    action_copy: {
        source: number;
        target: number;
    };
    query_available_groups: {};
    query_available_groups_result: {
        groups: GroupInfo[];
    };
    query_client_permissions: {};
    notify_client_permissions: {
        createTemplateGroup: boolean;
        createQueryGroup: boolean;
    };
    notify_destroy: {};
}
export declare function spawnModalGroupPermissionCopy(connection: ConnectionHandler, target: "channel" | "server", sourceGroup?: number, targetGroup?: number): void;
