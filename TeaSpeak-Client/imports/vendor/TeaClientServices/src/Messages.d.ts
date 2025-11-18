export declare type Message = {
    type: "Command";
    token: string;
    command: MessageCommand;
} | {
    type: "CommandResult";
    token: string | null;
    result: MessageCommandResult;
} | {
    type: "Notify";
    notify: MessageNotify;
};
export declare type MessageCommand = {
    type: "SessionInitialize";
    payload: CommandSessionInitialize;
} | {
    type: "SessionInitializeAgent";
    payload: CommandSessionInitializeAgent;
} | {
    type: "SessionUpdateLocale";
    payload: CommandSessionUpdateLocale;
} | {
    type: "InviteQueryInfo";
    payload: CommandInviteQueryInfo;
} | {
    type: "InviteLogAction";
    payload: CommandInviteLogAction;
} | {
    type: "InviteCreate";
    payload: CommandInviteCreate;
};
export declare type MessageCommandResult = {
    type: "Success";
} | {
    type: "GenericError";
    error: string;
} | {
    type: "ConnectionTimeout";
} | {
    type: "ConnectionClosed";
} | {
    type: "ClientSessionUninitialized";
} | {
    type: "ServerInternalError";
} | {
    type: "ParameterInvalid";
    parameter: string;
} | {
    type: "CommandParseError";
    error: string;
} | {
    type: "CommandEnqueueError";
    error: string;
} | {
    type: "CommandNotFound";
} | {
    type: "CommandNotImplemented";
} | {
    type: "SessionAlreadyInitialized";
} | {
    type: "SessionAgentAlreadyInitialized";
} | {
    type: "SessionNotInitialized";
} | {
    type: "SessionAgentNotInitialized";
} | {
    type: "SessionInvalidType";
} | {
    type: "InviteSessionNotInitialized";
} | {
    type: "InviteSessionAlreadyInitialized";
} | {
    type: "InviteKeyInvalid";
    error: string;
} | {
    type: "InviteKeyNotFound";
} | {
    type: "InviteKeyExpired";
};
export declare type MessageCommandErrorResult = Exclude<MessageCommandResult, {
    type: "Success";
}>;
export declare type MessageNotify = {
    type: "NotifyClientsOnline";
    payload: NotifyClientsOnline;
} | {
    type: "NotifyInviteCreated";
    payload: NotifyInviteCreated;
} | {
    type: "NotifyInviteInfo";
    payload: NotifyInviteInfo;
};
export declare type InviteAction = {
    type: "OpenTeaClientProtocol";
} | {
    type: "RedirectWebClient";
} | {
    type: "ConnectAttempt";
} | {
    type: "ConnectSuccess";
} | {
    type: "ConnectFailure";
    payload: {
        reason: string;
    };
} | {
    type: "ConnectNoAction";
    payload: {
        reason: string;
    };
};
export declare enum ClientSessionType {
    WebClient = 0,
    TeaClient = 1,
    InviteWebSite = 16
}
export declare type CommandSessionInitialize = {
    anonymize_ip: boolean;
};
export declare type CommandSessionInitializeAgent = {
    session_type: ClientSessionType;
    platform: string | null;
    platform_version: string | null;
    architecture: string | null;
    client_version: string | null;
    ui_version: string | null;
};
export declare type CommandSessionUpdateLocale = {
    ip_country: string | null;
    selected_locale: string | null;
    local_timestamp: number;
};
export declare type CommandInviteQueryInfo = {
    link_id: string;
    register_view: boolean;
};
export declare type CommandInviteLogAction = {
    link_id: string;
    action: InviteAction;
};
export declare type CommandInviteCreate = {
    new_link: boolean;
    properties_connect: {
        [key: string]: string;
    };
    properties_info: {
        [key: string]: string;
    };
    timestamp_expired: number;
};
export declare type NotifyClientsOnline = {
    users_online: {
        [key: number]: number;
    };
    unique_users_online: {
        [key: number]: number;
    };
    total_users_online: number;
    total_unique_users_online: number;
};
export declare type NotifyInviteCreated = {
    link_id: string;
    admin_token: string | null;
};
export declare type NotifyInviteInfo = {
    link_id: string;
    timestamp_created: number;
    timestamp_deleted: number;
    timestamp_expired: number;
    amount_viewed: number;
    amount_clicked: number;
    properties_connect: {
        [key: string]: string;
    };
    properties_info: {
        [key: string]: string;
    };
};
