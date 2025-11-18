import * as React from "react";
export declare enum BatchUpdateType {
    UNSET = -1,
    GENERAL = 0,
    CHANNEL_TREE = 1
}
export declare function BatchUpdateAssignment(type: BatchUpdateType): (constructor: Function) => void;
export declare abstract class ReactComponentBase<Properties, State> extends React.Component<Properties, State> {
    private update_batch;
    private batch_component_id;
    private batch_component_force_id;
    constructor(props: Properties);
    protected initialize(): void;
    protected defaultState(): State;
    setState<K extends keyof State>(state: ((prevState: Readonly<State>, props: Readonly<Properties>) => (Pick<State, K> | State | null)) | (Pick<State, K> | State | null), callback?: () => void): void;
    forceUpdate(callback?: () => void): void;
    protected classList(...classes: (string | undefined)[]): string;
    protected hasChildren(): boolean;
}
export declare function batch_updates(type: BatchUpdateType): void;
export declare function flush_batched_updates(type: BatchUpdateType, force?: boolean): void;
