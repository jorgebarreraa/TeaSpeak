export type ProxiedEvents<EventObject> = {
    [Q in keyof EventObject]: EventObject[Q] extends (...args: any) => void ? (...args: Parameters<EventObject[Q]>) => void : never
}

export type FunctionalInterface<ObjectType> = {
    [P in keyof ObjectType]: ObjectType[P] extends (...args: any) => Promise<any> ? (...args: any) => Promise<any> :
                             P extends "events" ? ObjectType[P] extends ProxiedEvents<ObjectType[P]> ? ProxiedEvents<ObjectType[P]> : never : never
};

export type ProxiedClassProperties = { instanceId: string, ownerWindowId: number, events: any };

export type ProxyInterface<ObjectType> = FunctionalInterface<ObjectType>;
export type ProxyClass<ObjectType> = { new(): ProxyInterface<ObjectType> & ProxiedClass<ObjectType> };

let constructorProperties: ProxiedClassProperties;
export abstract class ProxiedClass<Interface extends { events?: ProxiedEvents<Interface["events"]> }> {
    public readonly ownerWindowId: number;
    public readonly instanceId: string;

    public readonly events: ProxiedEvents<Interface["events"]>;

    public constructor() {
        if(typeof constructorProperties === "undefined") {
            throw "a ProxiedClass instance can only be allocated by createProxiedClassInstance";
        }

        this.ownerWindowId = constructorProperties.ownerWindowId;
        this.instanceId = constructorProperties.instanceId;
        this.events = constructorProperties.events;
        constructorProperties = undefined;
    }

    public destroy() {}
}

export function createProxiedClassInstance<T>(klass: new (...args: any[]) => ProxiedClass<T> & ProxyInterface<T>, args: any[], props: ProxiedClassProperties) : ProxiedClass<T> & ProxyInterface<T> {
    constructorProperties = props;
    try {
        const result = new klass(...args);
        if(typeof constructorProperties !== "undefined") {
            throw "tried to allocate an class which didn't called the ProxiedClass constructor";
        }
        return result;
    } finally {
        constructorProperties = undefined;
    }
}

export function generateUUID() {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
        var r = Math.random() * 16 | 0, v = c == 'x' ? r : (r & 0x3 | 0x8);
        return v.toString(16);
    });
}