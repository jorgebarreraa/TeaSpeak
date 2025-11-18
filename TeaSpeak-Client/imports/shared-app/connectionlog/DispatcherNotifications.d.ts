import { EventType, TypeInfo } from "tc-shared/connectionlog/Definitions";
export declare type DispatcherLog<T extends keyof TypeInfo> = (data: TypeInfo[T], handlerId: string, eventType: T) => void;
export declare function findNotificationDispatcher<T extends keyof TypeInfo>(type: T): DispatcherLog<T>;
export declare function getRegisteredNotificationDispatchers(): TypeInfo[];
export declare function isNotificationEnabled(type: EventType): any;
