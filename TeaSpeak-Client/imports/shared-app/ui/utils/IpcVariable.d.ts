import { UiVariableConsumer, UiVariableMap, UiVariableProvider } from "tc-shared/ui/utils/Variable";
export declare class IpcUiVariableProvider<Variables extends UiVariableMap> extends UiVariableProvider<Variables> {
    readonly ipcChannelId: string;
    private broadcastChannel;
    constructor();
    destroy(): void;
    protected doSendVariable(variable: string, customData: any, value: any): void;
    private handleIpcMessage;
    generateConsumerDescription(): IpcVariableDescriptor<Variables>;
}
export declare type IpcVariableDescriptor<Variables extends UiVariableMap> = {
    readonly ipcChannelId: string;
};
declare class IpcUiVariableConsumer<Variables extends UiVariableMap> extends UiVariableConsumer<Variables> {
    readonly description: IpcVariableDescriptor<Variables>;
    private broadcastChannel;
    private editListener;
    constructor(description: IpcVariableDescriptor<Variables>);
    destroy(): void;
    protected doEditVariable(variable: string, customData: any, newValue: any): Promise<void> | void;
    protected doRequestVariable(variable: string, customData: any): void;
    private handleIpcMessage;
}
export declare function createIpcUiVariableProvider<Variables extends UiVariableMap>(): IpcUiVariableProvider<Variables>;
export declare function createIpcUiVariableConsumer<Variables extends UiVariableMap>(description: IpcVariableDescriptor<Variables>): IpcUiVariableConsumer<Variables>;
export {};
