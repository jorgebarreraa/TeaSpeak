/// <reference types="node" />
import "jsrender";
declare global {
    function setInterval(handler: TimerHandler, timeout?: number, ...arguments: any[]): number;
    function setTimeout(handler: TimerHandler, timeout?: number, ...arguments: any[]): number;
    interface Array<T> {
        remove(elem?: T): boolean;
        last?(): T;
        pop_front(): T | undefined;
        /**
         * @param entry The entry to toggle
         * @returns `true` if the entry has been inserted and false if the entry has been deleted
         */
        toggle(entry: T): boolean;
        /**
         * @param entry The entry to toggle
         * @param insert Whatever the entry should be in the array or not
         * @returns `true` if the array has been modified
         */
        toggle(entry: T, insert: boolean): any;
    }
    interface JSON {
        map_to<T>(object: T, json: any, variables?: string | string[], validator?: (map_field: string, map_value: string) => boolean, variable_direction?: number): number;
        map_field_to<T>(object: T, value: any, field: string): boolean;
    }
    type JQueryScrollType = "height" | "width";
    interface JQuery<TElement = HTMLElement> {
        renderTag(values?: any): JQuery<TElement>;
        hasScrollBar(direction?: JQueryScrollType): boolean;
        visible_height(): number;
        visible_width(): number;
        firstParent(selector: string): JQuery;
    }
    interface JQueryStatic<TElement extends Node = HTMLElement> {
        spawn<K extends keyof HTMLElementTagNameMap>(tagName: K): JQuery<HTMLElementTagNameMap[K]>;
    }
    interface Window {
        __REACT_DEVTOOLS_GLOBAL_HOOK__: any;
        readonly webkitAudioContext: typeof AudioContext;
        readonly AudioContext: typeof OfflineAudioContext;
        readonly OfflineAudioContext: typeof OfflineAudioContext;
        readonly webkitOfflineAudioContext: typeof OfflineAudioContext;
        readonly RTCPeerConnection: typeof RTCPeerConnection;
        readonly Pointer_stringify: any;
        readonly require: typeof require;
    }
    const __non_webpack_require__: typeof require;
    interface Navigator {
        browserSpecs: {
            name: string;
            version: string;
        };
        mozGetUserMedia(constraints: MediaStreamConstraints, successCallback: NavigatorUserMediaSuccessCallback, errorCallback: NavigatorUserMediaErrorCallback): void;
        webkitGetUserMedia(constraints: MediaStreamConstraints, successCallback: NavigatorUserMediaSuccessCallback, errorCallback: NavigatorUserMediaErrorCallback): void;
    }
    interface ObjectConstructor {
        isSimilar(a: any, b: any): boolean;
    }
}
export declare type IfEquals<X, Y, A = X, B = never> = (<T>() => T extends X ? 1 : 2) extends (<T>() => T extends Y ? 1 : 2) ? A : B;
export declare type WritableKeys<T> = {
    [P in keyof T]-?: IfEquals<{
        [Q in P]: T[P];
    }, {
        -readonly [Q in P]: T[P];
    }, P, never>;
}[keyof T];
export declare type ReadonlyKeys<T> = {
    [P in keyof T]: IfEquals<{
        [Q in P]: T[P];
    }, {
        -readonly [Q in P]: T[P];
    }, never, P>;
}[keyof T];
export declare function crashOnThrow<T>(promise: Promise<T> | (() => Promise<T>)): Promise<T>;
export declare function ignorePromise<T>(_promise: Promise<T>): void;
export declare function NoThrow(target: any, methodName: string, descriptor: PropertyDescriptor): void;
export declare function CallOnce(target: any, methodName: string, descriptor: PropertyDescriptor): void;
export declare function NonNull(target: any, methodName: string, parameterIndex: number): void;
/**
 * The class or method has been constrained
 */
export declare function ParameterConstrained(target: any, methodName: string, descriptor: PropertyDescriptor): void;
