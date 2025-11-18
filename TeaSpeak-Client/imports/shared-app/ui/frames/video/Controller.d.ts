import { ConnectionHandler } from "tc-shared/ConnectionHandler";
export declare class ChannelVideoFrame {
    private readonly handle;
    private readonly events;
    private container;
    private controller;
    constructor(handle: ConnectionHandler);
    destroy(): void;
    getContainer(): HTMLDivElement;
}
