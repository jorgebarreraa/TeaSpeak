import { ReadonlyKeys, WritableKeys } from "tc-shared/proto";
export declare type UiVariable = Transferable | undefined | null | number | string | object;
export declare type UiVariableMap = {
    [key: string]: any;
};
export declare type ReadonlyVariables<Variables extends UiVariableMap> = Pick<Variables, ReadonlyKeys<Variables>>;
export declare type WriteableVariables<Variables extends UiVariableMap> = Pick<Variables, WritableKeys<Variables>>;
declare type UiVariableEditor<Variables extends UiVariableMap, T extends keyof Variables> = Variables[T] extends {
    __readonly: any;
} ? never : (newValue: Variables[T], customData: any) => Variables[T] | void | boolean;
declare type UiVariableEditorPromise<Variables extends UiVariableMap, T extends keyof Variables> = Variables[T] extends {
    __readonly: any;
} ? never : (newValue: Variables[T], customData: any) => Promise<Variables[T] | void | boolean>;
export declare abstract class UiVariableProvider<Variables extends UiVariableMap> {
    private variableProvider;
    private variableEditor;
    private artificialDelay;
    protected constructor();
    destroy(): void;
    getArtificialDelay(): number;
    setArtificialDelay(value: number): void;
    setVariableProvider<T extends keyof Variables>(variable: T, provider: (customData: any) => Variables[T] | Promise<Variables[T]>): void;
    setVariableProviderAsync<T extends keyof Variables>(variable: T, provider: (customData: any) => Promise<Variables[T]>): void;
    /**
     * @param variable
     * @param editor If the editor returns `false` or a new variable, such variable will be used
     */
    setVariableEditor<T extends keyof Variables>(variable: T, editor: UiVariableEditor<Variables, T>): void;
    setVariableEditorAsync<T extends keyof Variables>(variable: T, editor: UiVariableEditorPromise<Variables, T>): void;
    /**
     * Send/update a variable
     * @param variable The target variable to send.
     * @param customData
     * @param forceSend If `true` the variable will be send event though it hasn't changed.
     */
    sendVariable<T extends keyof Variables>(variable: T, customData?: any, forceSend?: boolean): void | Promise<void>;
    getVariable<T extends keyof Variables>(variable: T, customData?: any, ignoreCache?: boolean): Promise<Variables[T]>;
    getVariableSync<T extends keyof Variables>(variable: T, customData?: any, ignoreCache?: boolean): Variables[T];
    protected resolveVariable(variable: string, customData: any): Promise<any> | any;
    protected doEditVariable(variable: string, customData: any, newValue: any): Promise<void> | void;
    private handleEditResult;
    private handleEditError;
    protected abstract doSendVariable(variable: string, customData: any, value: any): any;
}
export declare type UiVariableStatus<Variables extends UiVariableMap, T extends keyof Variables> = {
    status: "loading";
    localValue: Variables[T] | undefined;
    remoteValue: undefined;
    setValue: (newValue: Variables[T], localOnly?: boolean) => void;
} | {
    status: "loaded" | "applying";
    localValue: Variables[T];
    remoteValue: Variables[T];
    setValue: (newValue: Variables[T], localOnly?: boolean) => void;
};
export declare type UiReadOnlyVariableStatus<Variables extends UiVariableMap, T extends keyof Variables> = {
    status: "loading" | "loaded";
    value: Variables[T];
};
export declare abstract class UiVariableConsumer<Variables extends UiVariableMap> {
    private variableCache;
    destroy(): void;
    private getOrCreateVariable;
    private derefVariable;
    setVariable<T extends keyof WriteableVariables<Variables>>(variable: T, customData: any, newValue: Variables[T]): void;
    useVariable<T extends keyof WriteableVariables<Variables>>(variable: T, customData?: any, defaultValue?: Variables[T]): UiVariableStatus<Variables, T>;
    useReadOnly<T extends keyof Variables>(variable: T, customData?: any, defaultValue?: never): UiReadOnlyVariableStatus<Variables, T>;
    useReadOnly<T extends keyof Variables>(variable: T, customData: any | undefined, defaultValue: Variables[T]): Variables[T];
    protected notifyRemoteVariable(variable: string, customData: any | undefined, value: any): void;
    protected abstract doRequestVariable(variable: string, customData: any | undefined): any;
    protected abstract doEditVariable(variable: string, customData: any | undefined, value: any): Promise<void> | void;
}
export {};
