import {ipcRenderer, IpcRendererEvent, remote} from "electron";
import {ProxiedClass, ProxyInterface} from "./Definitions";

//@ts-ignore
import {tr} from "tc-shared/i18n/localize";

//@ts-ignore
import {LogCategory, logError, logWarn} from "tc-shared/log";

export class ObjectProxyClient<ObjectType extends ProxyInterface<ObjectType>> {
    private readonly ipcChannel: string;
    private readonly handleIPCMessageBinding;

    private eventInvokers: {[key: string]: { fireEvent: (type: string, ...args: any) => void }} = {};

    constructor(ipcChannel: string) {
        this.ipcChannel = ipcChannel;
        this.handleIPCMessageBinding = this.handleIPCMessage.bind(this);
    }

    initialize() {
        ipcRenderer.on(this.ipcChannel, this.handleIPCMessageBinding);
    }

    destroy() {
        ipcRenderer.off(this.ipcChannel, this.handleIPCMessageBinding);
        /* TODO: Destroy all client instances? */
    }

    async createNewInstance() : Promise<ObjectType & ProxiedClass<ObjectType>> {
        let object = {
            objectId: undefined as string
        };

        const result = await ipcRenderer.invoke(this.ipcChannel, "create");
        if(result.status !== "success") {
            if(result.status === "error") {
                throw result.message || tr("failed to create a new instance");
            } else {
                throw tr("failed to create a new object instance ({})", result.status);
            }
        }
        object.objectId = result.instanceId;

        const ipcChannel = this.ipcChannel;

        const events = this.generateEvents(object.objectId);
        return new Proxy(object, {
            get(target, key: PropertyKey) {
                if(key === "ownerWindowId") {
                    return remote.getCurrentWindow().id;
                } else if(key === "instanceId") {
                    return object.objectId;
                } else if(key === "destroy") {
                    return () => {
                        ipcRenderer.invoke(ipcChannel, "destroy", target.objectId);
                        events.destroy();
                    };
                } else if(key === "events") {
                    return events;
                } else if(key === "then" || key === "catch") {
                    /* typescript for some reason has an issue if then and catch return anything */
                    return undefined;
                }

                return (...args: any) => ipcRenderer.invoke(ipcChannel, "invoke", target.objectId, key, ...args);
            },

            set(): boolean {
                throw "class is a ready only interface";
            }
        }) as any;
    }

    private generateEvents(objectId: string) : { destroy() } {
        const eventInvokers = this.eventInvokers;
        const registeredEvents = {};

        eventInvokers[objectId] = {
            fireEvent(event: string, ...args: any) {
                if(typeof registeredEvents[event] === "undefined")
                    return;

                try {
                    registeredEvents[event](...args);
                } catch (error) {
                    logError(LogCategory.IPC, tr("Failed to invoke event %s on %s: %o"), event, objectId, error);
                }
            }
        };

        return new Proxy({ }, {
            set(target, key: PropertyKey, value: any): boolean {
                registeredEvents[key] = value;
                return true;
            },

            get(target, key: PropertyKey): any {
                if(key === "destroy") {
                    return () => delete eventInvokers[objectId];
                } else if(typeof registeredEvents[key] === "function") {
                    return () => { throw tr("events can only be invoked via IPC") };
                } else {
                    return undefined;
                }
            }
        }) as any;
    }

    private handleIPCMessage(_event: IpcRendererEvent, ...args: any[]) {
        const actionType = args[0];

        if(actionType === "notify-event") {
            const invoker = this.eventInvokers[args[1]];
            if(typeof invoker !== "object") {
                logWarn(LogCategory.IPC, tr("Received event %s for unknown object instance on channel %s"), args[2], args[1]);
                return;
            }

            invoker.fireEvent(args[2], ...args.slice(3));
        }
    }
}