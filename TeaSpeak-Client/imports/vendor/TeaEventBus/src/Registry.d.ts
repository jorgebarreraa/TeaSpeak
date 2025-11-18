import { Event, EventConsumer, EventMap, EventSender } from "./Events";
import { IpcRegistryDescription } from "./Ipc";
import * as React from "react";
export declare class Registry<Events extends EventMap<Events> = EventMap<any>> implements EventSender<Events> {
    protected readonly registryUniqueId: any;
    protected persistentEventHandler: {
        [key: string]: ((event: any) => void)[];
    };
    protected oneShotEventHandler: {
        [key: string]: ((event: any) => void)[];
    };
    protected genericEventHandler: ((event: any) => void)[];
    protected consumer: EventConsumer[];
    private ipcConsumer;
    private debugPrefix;
    private warnUnhandledEvents;
    private pendingAsyncCallbacks;
    private pendingAsyncCallbacksTimeout;
    private pendingReactCallbacks;
    private pendingReactCallbacksFrame;
    static fromIpcDescription<Events extends EventMap<Events> = EventMap<any>>(description: IpcRegistryDescription<Events>): Registry<Events>;
    constructor();
    destroy(): void;
    enableDebug(prefix: string): void;
    disableDebug(): void;
    enableWarnUnhandledEvents(): void;
    disableWarnUnhandledEvents(): void;
    fire<T extends keyof Events>(eventType: T, data?: Events[T], overrideTypeKey?: boolean): void;
    fire_later<T extends keyof Events>(eventType: T, data?: Events[T], callback?: () => void): void;
    fire_react<T extends keyof Events>(eventType: T, data?: Events[T], callback?: () => void): void;
    on<T extends keyof Events>(event: T | T[], handler: (event: Event<Events, T>) => void): () => void;
    one<T extends keyof Events>(event: T | T[], handler: (event: Event<Events, T>) => void): () => void;
    off(handler: (event: Event<Events, keyof Events>) => void): any;
    off<T extends keyof Events>(events: T | T[], handler: (event: Event<Events, T>) => void): any;
    onAll(handler: (event: Event<Events, keyof Events>) => void): () => void;
    offAll(handler: (event: Event<Events, keyof Events>) => void): void;
    /**
     * @param event
     * @param handler
     * @param condition If a boolean the event handler will only be registered if the condition is true
     * @param reactEffectDependencies
     */
    reactUse<T extends keyof Events>(event: T | T[], handler: (event: Event<Events, T>) => void, condition?: boolean, reactEffectDependencies?: any[]): any;
    private doInvokeEvent;
    private invokeAsyncCallbacks;
    private invokeReactCallbacks;
    registerHandler(handler: any, parentClasses?: boolean): void;
    unregisterHandler(handler: any): void;
    registerConsumer(consumer: EventConsumer): () => void;
    unregisterConsumer(consumer: EventConsumer): void;
    generateIpcDescription(): IpcRegistryDescription<Events>;
}
export declare function EventHandler<EventTypes>(events: (keyof EventTypes) | (keyof EventTypes)[]): (target: any, propertyKey: string, _descriptor: PropertyDescriptor) => void;
export declare function ReactEventHandler<ObjectClass = React.Component<any, any>, Events = any>(registry_callback: (object: ObjectClass) => Registry<Events>): (constructor: Function) => void;
