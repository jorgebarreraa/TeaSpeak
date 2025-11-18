import * as React from "react";
declare type EntryTagStyle = "text-only" | "normal";
export declare const ServerTag: React.MemoExoticComponent<(props: {
    serverName: string;
    handlerId: string;
    serverUniqueId?: string;
    className?: string;
    style?: EntryTagStyle;
}) => JSX.Element>;
export declare const ClientTag: React.MemoExoticComponent<(props: {
    clientName: string;
    clientUniqueId: string;
    handlerId: string;
    clientId?: number;
    clientDatabaseId?: number;
    className?: string;
    style?: EntryTagStyle;
}) => JSX.Element>;
export declare const ChannelTag: React.MemoExoticComponent<(props: {
    channelName: string;
    channelId: number;
    handlerId: string;
    className?: string;
}) => JSX.Element>;
export {};
