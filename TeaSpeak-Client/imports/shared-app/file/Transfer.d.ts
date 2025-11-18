import { Registry } from "../events";
import { CommandResult } from "../connection/ServerConnectionDeclaration";
import { ErrorCode } from "../connection/ErrorCode";
export declare enum TransferSourceType {
    BROWSER_FILE = 0,
    BUFFER = 1,
    TEXT = 2
}
export declare abstract class TransferSource {
    readonly type: TransferSourceType;
    protected constructor(type: TransferSourceType);
    abstract fileSize(): Promise<number>;
}
export declare abstract class BrowserFileTransferSource extends TransferSource {
    protected constructor();
    abstract getFile(): File;
}
export declare abstract class BufferTransferSource extends TransferSource {
    protected constructor();
    abstract getBuffer(): ArrayBuffer;
}
export declare abstract class TextTransferSource extends TransferSource {
    protected constructor();
    abstract getText(): string;
}
export declare type TransferSourceSupplier = (transfer: FileUploadTransfer) => Promise<TransferSource>;
export declare enum TransferTargetType {
    RESPONSE = 0,
    DOWNLOAD = 1,
    FILE = 2
}
export declare abstract class TransferTarget {
    readonly type: TransferTargetType;
    protected constructor(type: TransferTargetType);
}
export declare abstract class DownloadTransferTarget extends TransferTarget {
    protected constructor();
}
export declare abstract class ResponseTransferTarget extends TransferTarget {
    protected constructor();
    abstract hasResponse(): boolean;
    abstract getResponse(): Response;
}
export declare abstract class FileTransferTarget extends TransferTarget {
    protected constructor();
    abstract getFilePath(): string;
    abstract hasFileName(): boolean;
    abstract getFileName(): string;
}
export declare type TransferTargetSupplier = (transfer: FileDownloadTransfer) => Promise<TransferTarget>;
export declare enum FileTransferState {
    PENDING = 0,
    INITIALIZING = 1,
    CONNECTING = 2,
    RUNNING = 3,
    FINISHED = 4,
    ERRORED = 5,
    CANCELED = 6
}
export declare enum CancelReason {
    USER_ACTION = 0,
    SERVER_DISCONNECTED = 1
}
export declare enum FileTransferDirection {
    UPLOAD = 0,
    DOWNLOAD = 1
}
export interface FileTransferEvents {
    "notify_state_updated": {
        oldState: FileTransferState;
        newState: FileTransferState;
    };
    "notify_progress": {
        progress: TransferProgress;
    };
    "notify_transfer_canceled": {};
}
export interface TransferProperties {
    channel_id: number | 0;
    path: string;
    name: string;
}
export interface InitializedTransferProperties {
    serverTransferId: number;
    transferKey: string;
    addresses: {
        serverAddress: string;
        serverPort: number;
    }[];
    protocol: number;
    seekOffset: number;
    fileSize?: number;
}
export interface TransferInitializeError {
    error: "initialize";
    commandResult: string | CommandResult;
}
export interface TransferConnectError {
    error: "connection";
    reason: "missing-provider" | "provider-initialize-error" | "handle-initialize-error" | "network-error";
    extraMessage?: string;
}
export interface TransferIOError {
    error: "io";
    reason: "unsupported-target" | "failed-to-initialize-target" | "buffer-transfer-failed";
    extraMessage?: string;
}
export interface TransferErrorStatus {
    error: "status";
    status: ErrorCode;
    extraMessage: string;
}
export interface TransferErrorTimeout {
    error: "timeout";
}
export declare type TransferErrorType = TransferInitializeError | TransferConnectError | TransferIOError | TransferErrorStatus | TransferErrorTimeout;
export interface TransferProgress {
    timestamp: number;
    file_bytes_transferred: number;
    file_current_offset: number;
    file_start_offset: number;
    file_total_size: number;
    network_bytes_received: number;
    network_bytes_send: number;
    network_current_speed: number;
    network_average_speed: number;
}
export interface TransferTimings {
    timestampScheduled: number;
    timestampExecuted: number;
    timestampTransferBegin: number;
    timestampEnd: number;
}
export interface FinishedFileTransfer {
    readonly clientTransferId: number;
    readonly timings: TransferTimings;
    readonly properties: TransferProperties;
    readonly direction: FileTransferDirection;
    readonly state: FileTransferState.CANCELED | FileTransferState.FINISHED | FileTransferState.ERRORED;
    readonly transferError?: TransferErrorType;
    readonly transferErrorMessage?: string;
    readonly bytesTransferred: number;
}
export declare class FileTransfer {
    readonly events: Registry<FileTransferEvents>;
    readonly clientTransferId: number;
    readonly direction: FileTransferDirection;
    readonly properties: TransferProperties;
    readonly timings: TransferTimings;
    lastStateUpdate: number;
    private cancelReason;
    private transferProperties_;
    private transferError_;
    private transferErrorMessage_;
    private transferState_;
    private progress_;
    protected constructor(direction: any, clientTransferId: any, properties: any);
    destroy(): void;
    isRunning(): boolean;
    isPending(): boolean;
    isFinished(): boolean;
    transferState(): FileTransferState;
    transferProperties(): InitializedTransferProperties | undefined;
    currentError(): TransferErrorType | undefined;
    currentErrorMessage(): string | undefined;
    lastProgressInfo(): TransferProgress | undefined;
    setFailed(error: TransferErrorType, asMessage: string): void;
    setProperties(properties: InitializedTransferProperties): void;
    requestCancel(reason: CancelReason): void;
    setTransferState(newState: FileTransferState): void;
    updateProgress(progress: TransferProgress): void;
    awaitFinished(): Promise<void>;
}
export declare class FileDownloadTransfer extends FileTransfer {
    readonly targetSupplier: TransferTargetSupplier;
    target: TransferTarget;
    constructor(clientTransferId: any, properties: TransferProperties, targetSupplier: any);
}
export declare class FileUploadTransfer extends FileTransfer {
    readonly sourceSupplier: TransferSourceSupplier;
    source: TransferSource;
    fileSize: number;
    constructor(clientTransferId: any, properties: TransferProperties, sourceSupplier: any);
}
export declare abstract class TransferProvider {
    private static instance_;
    static provider(): TransferProvider;
    static setProvider(provider: TransferProvider): void;
    abstract executeFileDownload(transfer: FileDownloadTransfer): any;
    abstract executeFileUpload(transfer: FileUploadTransfer): any;
    abstract targetSupported(type: TransferTargetType): any;
    abstract sourceSupported(type: TransferSourceType): any;
    createResponseTarget(): Promise<ResponseTransferTarget>;
    createDownloadTarget(filename?: string): Promise<DownloadTransferTarget>;
    createFileTarget(path?: string, filename?: string): Promise<FileTransferTarget>;
    createBufferSource(buffer: ArrayBuffer): Promise<BufferTransferSource>;
    createTextSource(text: string): Promise<TextTransferSource>;
    createBrowserFileSource(file: File): Promise<BrowserFileTransferSource>;
}
