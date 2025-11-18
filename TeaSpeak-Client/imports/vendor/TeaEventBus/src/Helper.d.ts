import { EventMap, Event } from "./Events";
export declare function guid(): string;
export declare function arrayRemove(array: any[], element: any): boolean;
/**
 * Turn the payload object into a bus event object
 * @param payload
 */
export declare function createEvent<P extends EventMap<P>, T extends keyof P>(type: T, payload?: P[T]): Event<P, T>;
