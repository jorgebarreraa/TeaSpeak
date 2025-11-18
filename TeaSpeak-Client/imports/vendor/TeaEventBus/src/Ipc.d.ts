import { EventConsumer, EventDispatchType, EventMap } from "./Events";
import { Registry } from "./Registry";
export declare type IpcRegistryDescription<Events extends EventMap<Events> = EventMap<any>> = {
    ipcChannelId: string;
};
export declare class IpcEventBridge implements EventConsumer {
    readonly registry: Registry;
    readonly ipcChannelId: string;
    private readonly ownBridgeId;
    private broadcastChannel;
    constructor(registry: Registry, ipcChannelId: string | undefined);
    destroy(): void;
    handleEvent(dispatchType: EventDispatchType, eventType: string, eventPayload: any): void;
    private handleIpcMessage;
}
