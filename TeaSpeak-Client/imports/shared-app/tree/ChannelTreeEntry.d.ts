import { Registry } from "../events";
export interface ChannelTreeEntryEvents {
    notify_unread_state_change: {
        unread: boolean;
    };
}
export declare abstract class ChannelTreeEntry<Events extends ChannelTreeEntryEvents> {
    readonly events: Registry<Events>;
    readonly uniqueEntryId: number;
    protected selected_: boolean;
    protected unread_: boolean;
    protected constructor();
    setUnread(flag: boolean): void;
    isUnread(): boolean;
    abstract showContextMenu(pageX: number, pageY: number, on_close?: any): any;
}
