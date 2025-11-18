import { ConnectionHandler } from "tc-shared/ConnectionHandler";
import { CommandResult } from "tc-shared/connection/ServerConnectionDeclaration";
import { AvatarManager } from "tc-shared/file/LocalAvatars";
import { FileDownloadTransfer, FileTransfer, FileUploadTransfer, FinishedFileTransfer, TransferSourceSupplier, TransferTargetSupplier } from "tc-shared/file/Transfer";
import { Registry } from "tc-shared/events";
export declare enum FileType {
    DIRECTORY = 0,
    FILE = 1
}
export interface FileInfo {
    name: string;
    type: FileType;
    datetime: number;
    size: number;
    empty: boolean;
}
export declare type InitializeUploadOptions = {
    path: string;
    name: string;
    channel?: number;
    channelPassword?: string;
    source: TransferSourceSupplier;
};
export declare type InitializeDownloadOptions = {
    path: string;
    name: string;
    channel?: number;
    channelPassword?: string;
    targetSupplier: TransferTargetSupplier;
};
export interface FileManagerEvents {
    notify_transfer_registered: {
        transfer: FileTransfer;
    };
}
export declare class FileManager {
    private static readonly MAX_CONCURRENT_TRANSFERS;
    readonly connectionHandler: ConnectionHandler;
    readonly avatars: AvatarManager;
    readonly events: Registry<FileManagerEvents>;
    readonly finishedTransfers: FinishedFileTransfer[];
    private readonly commandHandler;
    private readonly registeredTransfers_;
    private clientTransferIdIndex;
    private scheduledTransferUpdate;
    private transerUpdateIntervalId;
    constructor(connection: any);
    destroy(): void;
    requestFileList(path: string, channelId?: number, channelPassword?: string): Promise<FileInfo[]>;
    requestFileInfo(files: {
        channelId?: number;
        channelPassword?: string;
        path: string;
    }[]): Promise<(FileInfo | CommandResult)[]>;
    deleteFile(props: {
        name: string;
        path?: string;
        cid?: number;
        cpw?: string;
    }): Promise<void>;
    deleteIcon(iconId: number): Promise<void>;
    registeredTransfers(): FileTransfer[];
    findTransfer(id: number): FileTransfer;
    findTransfer(channelId: number, path: string, name: string): FileTransfer;
    initializeFileDownload(options: InitializeDownloadOptions): FileDownloadTransfer;
    initializeFileUpload(options: InitializeUploadOptions): FileUploadTransfer;
    private registerTransfer;
    private scheduleTransferUpdate;
    private updateRegisteredTransfers;
}
