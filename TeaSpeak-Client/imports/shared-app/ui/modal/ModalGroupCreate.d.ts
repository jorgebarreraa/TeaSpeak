import { ConnectionHandler } from "tc-shared/ConnectionHandler";
export declare type GroupInfo = {
    id: number;
    name: string;
    type: "query" | "template" | "normal";
};
export interface GroupCreateModalEvents {
    action_set_name: {
        name: string | undefined;
    };
    action_set_type: {
        target: "query" | "template" | "normal";
    };
    action_set_source: {
        group: number;
    };
    action_cancel: {};
    action_create: {
        name: string;
        target: "query" | "template" | "normal";
        source: number;
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
export declare function spawnGroupCreate(connection: ConnectionHandler, target: "server" | "channel", sourceGroup?: number): void;
