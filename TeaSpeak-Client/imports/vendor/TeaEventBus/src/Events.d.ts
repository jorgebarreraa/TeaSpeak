export declare type EventPayloadObject = any;
export declare type EventMap<P> = {
    [K in keyof P]: EventPayloadObject & {
        type?: never;
    };
};
export declare type Event<P extends EventMap<P>, T extends keyof P> = {
    readonly type: T;
    as<S extends T>(target: S): Event<P, S>;
    asUnchecked<S extends T>(target: S): Event<P, S>;
    asAnyUnchecked<S extends keyof P>(target: S): Event<P, S>;
    /**
     * Return an object containing only the event payload specific key value pairs.
     */
    extractPayload(): P[T];
} & P[T];
export interface EventSender<Events extends EventMap<Events> = EventMap<any>> {
    fire<T extends keyof Events>(event_type: T, data?: Events[T], overrideTypeKey?: boolean): any;
    /**
     * Fire an event later by using setTimeout(..)
     * @param event_type The target event to be fired
     * @param data The payload of the event
     * @param callback The callback will be called after the event has been successfully dispatched
     */
    fire_later<T extends keyof Events>(event_type: T, data?: Events[T], callback?: () => void): any;
    /**
     * Fire an event, which will be delayed until the next animation frame.
     * This ensures that all react components have been successfully mounted/unmounted.
     * @param event_type The target event to be fired
     * @param data The payload of the event
     * @param callback The callback will be called after the event has been successfully dispatched
     */
    fire_react<T extends keyof Events>(event_type: T, data?: Events[T], callback?: () => void): any;
}
export declare type EventDispatchType = "sync" | "later" | "react";
export interface EventConsumer {
    handleEvent(mode: EventDispatchType, type: string, data: any): any;
}
