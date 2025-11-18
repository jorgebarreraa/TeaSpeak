import {BrowserWindow, ipcMain, IpcMainEvent} from "electron";
import {createProxiedClassInstance, generateUUID, ProxiedClass, ProxyClass, ProxyInterface} from "./Definitions";

export class ObjectProxyServer<ObjectType extends ProxyInterface<ObjectType>> {
    private readonly ipcChannel: string;
    private readonly klass: ProxyClass<ObjectType>;
    private readonly instances: { [key: string]: ProxyInterface<ObjectType> & ProxiedClass<ObjectType> } = {};

    private readonly handleIPCMessageBinding;

    constructor(ipcChannel: string, klass: ProxyClass<ObjectType>) {
        this.klass = klass;
        this.ipcChannel = ipcChannel;

        this.handleIPCMessageBinding = this.handleIPCMessage.bind(this);
    }

    initialize() {
        ipcMain.handle(this.ipcChannel, this.handleIPCMessageBinding);
    }

    destroy() {
        ipcMain.removeHandler(this.ipcChannel);
    }

    private async handleIPCMessage(event: IpcMainEvent, ...args: any[]) {
        const actionType = args[0];

        if(actionType === "create") {
            let instance: ProxiedClass<ObjectType> & ProxyInterface<ObjectType>;
            try {
                const instanceId = generateUUID();
                instance = createProxiedClassInstance<ObjectType>(this.klass, [], {
                    ownerWindowId: event.sender.id,
                    instanceId: instanceId,
                    events: this.generateEventProxy(instanceId, event.sender.id)
                });
                this.instances[instance.instanceId] = instance;
            } catch (error) {
                event.returnValue = { "status": "error", message: "create-error" };
                return;
            }

            return  { "status": "success", instanceId: instance.instanceId };
        } else {
            const instance = this.instances[args[1]];

            if(!instance) {
                throw "instance-unknown";
            }

            if(actionType === "destroy") {
                delete this.instances[args[1]];
                instance.destroy();
            } else if(actionType === "invoke") {
                if(typeof instance[args[2]] !== "function") {
                    throw "function-unknown";
                }

                return instance[args[2]](...args.slice(3));
            } else {
                console.warn("Received an invalid action: %s", actionType);
            }
        }
    }

    private generateEventProxy(instanceId: string, owningWindowId: number) : {} {
        const ipcChannel = this.ipcChannel;
        return new Proxy({ }, {
            get(target: { }, event: PropertyKey, _receiver: any): any {
                return (...args: any) => {
                    const window = BrowserWindow.fromId(owningWindowId);
                    if(!window) return;

                    window.webContents.send(ipcChannel, "notify-event", instanceId, event, ...args);
                }
            },
            set(): boolean {
                throw "the events are read only for the implementation";
            }
        })
    }
}