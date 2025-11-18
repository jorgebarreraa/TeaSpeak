import {
    BrowserFileTransferSource,
    BufferTransferSource,
    FileDownloadTransfer, FileTransfer,
    FileTransferState, FileTransferTarget,
    FileUploadTransfer,
    ResponseTransferTarget,
    TextTransferSource,
    TransferProvider,
    TransferSourceType,
    TransferTargetType
} from "tc-shared/file/Transfer";
import * as native from "tc-native/connection";
import {tr} from "tc-shared/i18n/localize";
import {LogCategory, logError} from "tc-shared/log";
import {base64_encode_ab} from "tc-shared/utils/buffers";
import * as path from "path";
import * as electron from "electron";

const executeTransfer = (transfer: FileTransfer, object: native.ft.TransferObject, callbackFinished: () => void) => {
    const address = transfer.transferProperties().addresses[0];
    let ntransfer: native.ft.NativeFileTransfer;
    try {
        ntransfer = native.ft.spawn_connection({
            client_transfer_id: transfer.clientTransferId,
            server_transfer_id: -1,
            object: object,
            remote_address: address.serverAddress,
            remote_port: address.serverPort,
            transfer_key: transfer.transferProperties().transferKey
        });
    } catch (error) {
        let message = typeof error === "object" ? 'message' in error ? error.message : typeof error === "string" ? error : undefined : undefined;
        if(!message) {
            logError(LogCategory.FILE_TRANSFER, tr("Failed to create file transfer handle: %o"), error);
        }

        transfer.setFailed({
            error: "connection",
            reason: "handle-initialize-error",
            extraMessage: message ? message : tr("Lookup the console")
        }, message ? message : tr("Lookup the console"));
        return;
    }

    ntransfer.callback_start = () => {
        if(!transfer.isFinished()) {
            transfer.setTransferState(FileTransferState.RUNNING);
            transfer.lastStateUpdate = Date.now();
        }
    };

    ntransfer.callback_failed = error => {
        if(transfer.isFinished()) {
            return;
        }

        transfer.lastStateUpdate = Date.now();
        transfer.setFailed({
            error: "connection",
            reason: "network-error",
            extraMessage: error
        }, error);
    };

    ntransfer.callback_finished = aborted => {
        if(transfer.isFinished()) {
            return;
        }

        callbackFinished();
        transfer.setTransferState(aborted ? FileTransferState.CANCELED : FileTransferState.FINISHED);
        transfer.lastStateUpdate = Date.now();
    };

    ntransfer.callback_progress = (current, max) => {
        if(transfer.isFinished()) {
            return;
        }

        const transferInfo = transfer.lastProgressInfo();
        /* ATTENTION: transferInfo.timestamp | 0 does not work since 1591875114970 > 2^32 (1591875114970 | 0 => -1557751846) */
        if(transferInfo && Date.now() - (typeof transferInfo.timestamp === "number" ? transferInfo.timestamp : 0) < 2000 && !(transferInfo as any).native_info) {
            return;
        }

        transfer.updateProgress({
            network_current_speed: 0,
            network_average_speed: 0,

            network_bytes_send: 0,
            network_bytes_received: 0,

            file_current_offset: current,
            file_total_size: max,
            file_bytes_transferred: current,
            file_start_offset: 0,
            timestamp: Date.now(),

            native_info: true
        } as any);
        transfer.lastStateUpdate = Date.now();
    };

    try {
        if(!ntransfer.start()) {
            throw tr("failed to start transfer");
        }
    } catch (error) {
        if(typeof error !== "string") {
            logError(LogCategory.FILE_TRANSFER, tr("Failed to start file transfer: %o"), error);
        }

        transfer.setFailed({
            error: "connection",
            reason: "network-error",
            extraMessage: typeof error === "string" ? error : tr("Lookup the console")
        }, typeof error === "string" ? error : tr("Lookup the console"));
        return;
    }
};

TransferProvider.setProvider(new class extends TransferProvider {
    executeFileDownload(transfer: FileDownloadTransfer) {
        try {
            if(!transfer.target) {
                throw tr("transfer target is undefined");
            }
            transfer.setTransferState(FileTransferState.CONNECTING);

            let nativeTarget: native.ft.FileTransferTarget;
            if(transfer.target instanceof ResponseTransferTargetImpl) {
                transfer.target.initialize(transfer.transferProperties().fileSize);
                nativeTarget = transfer.target.nativeTarget;
            } else if(transfer.target instanceof FileTransferTargetImpl) {
                nativeTarget = transfer.target.getNativeTarget(transfer.properties.name, transfer.transferProperties().fileSize);
            } else {
                transfer.setFailed({
                    error: "io",
                    reason: "unsupported-target"
                }, tr("invalid transfer target type"));
                return;
            }

            executeTransfer(transfer, nativeTarget, () => {
                if(transfer.target instanceof ResponseTransferTargetImpl) {
                    transfer.target.createResponseFromBuffer();
                }
            });
        } catch (error) {
            if(typeof error !== "string") {
                logError(LogCategory.FILE_TRANSFER, tr("Failed to initialize transfer target: %o"), error);
            }

            transfer.setFailed({
                error: "io",
                reason: "failed-to-initialize-target",
                extraMessage: typeof error === "string" ? error : tr("Lookup the console")
            }, typeof error === "string" ? error : tr("Lookup the console"));
        }
    }

    executeFileUpload(transfer: FileUploadTransfer) {
        try {
            if(!transfer.source) {
                throw tr("transfer source is undefined");
            }

            let nativeSource: native.ft.FileTransferSource;
            if(transfer.source instanceof BrowserFileTransferSourceImpl) {
                nativeSource = transfer.source.getNativeSource();
            } else if(transfer.source instanceof TextTransferSourceImpl) {
                nativeSource = transfer.source.getNativeSource();
            } else if(transfer.source instanceof BufferTransferSourceImpl) {
                nativeSource = transfer.source.getNativeSource();
            } else {
                transfer.setFailed({
                    error: "io",
                    reason: "unsupported-target"
                }, tr("invalid transfer target type"));
                return;
            }

            executeTransfer(transfer, nativeSource, () => { });
        } catch (error) {
            if(typeof error !== "string") {
                logError(LogCategory.FILE_TRANSFER, tr("Failed to initialize transfer source: %o"), error);
            }

            transfer.setFailed({
                error: "io",
                reason: "failed-to-initialize-target",
                extraMessage: typeof error === "string" ? error : tr("Lookup the console")
            }, typeof error === "string" ? error : tr("Lookup the console"));
        }
    }

    sourceSupported(type: TransferSourceType) {
        switch (type) {
            case TransferSourceType.TEXT:
            case TransferSourceType.BUFFER:
            case TransferSourceType.BROWSER_FILE:
                return true;

            default:
                return false;
        }
    }

    targetSupported(type: TransferTargetType) {
        switch (type) {
            case TransferTargetType.RESPONSE:
            case TransferTargetType.FILE:
                return true;

            case TransferTargetType.DOWNLOAD:
            default:
                return false;
        }
    }

    async createTextSource(text: string): Promise<TextTransferSource> {
        return new TextTransferSourceImpl(text);
    }

    async createBufferSource(buffer: ArrayBuffer): Promise<BufferTransferSource> {
        return new BufferTransferSourceImpl(buffer);
    }

    async createBrowserFileSource(file: File): Promise<BrowserFileTransferSource> {
        return new BrowserFileTransferSourceImpl(file);
    }

    async createResponseTarget(): Promise<ResponseTransferTarget> {
        return new ResponseTransferTargetImpl();
    }

    async createFileTarget(path?: string, name?: string): Promise<FileTransferTarget> {
        const target = new FileTransferTargetImpl(path, name);
        await target.requestPath();
        return target;
    }
});

class TextTransferSourceImpl extends TextTransferSource {
    private readonly text: string;
    private buffer: ArrayBuffer;
    private nativeSource: native.ft.FileTransferSource;

    constructor(text: string) {
        super();
        this.text = text;
    }

    getText(): string {
        return this.text;
    }


    async fileSize(): Promise<number> {
        return this.getArrayBuffer().byteLength;
    }

    getArrayBuffer() : ArrayBuffer {
        if(this.buffer) return this.buffer;

        const encoder = new TextEncoder();
        this.buffer = encoder.encode(this.text);
        return this.buffer;
    }

    getNativeSource() {
        if(this.nativeSource) return this.nativeSource;

        this.nativeSource = native.ft.upload_transfer_object_from_buffer(this.getArrayBuffer());
        return this.nativeSource;
    }
}

class BufferTransferSourceImpl extends BufferTransferSource {
    private readonly buffer: ArrayBuffer;
    private nativeSource: native.ft.FileTransferSource;

    constructor(buffer: ArrayBuffer) {
        super();
        this.buffer = buffer;
    }


    async fileSize(): Promise<number> {
        return this.buffer.byteLength;
    }

    getNativeSource() {
        if(this.nativeSource) return this.nativeSource;

        this.nativeSource = native.ft.upload_transfer_object_from_buffer(this.buffer);
        return this.nativeSource;
    }

    getBuffer(): ArrayBuffer {
        return this.buffer;
    }
}

class BrowserFileTransferSourceImpl extends BrowserFileTransferSource {
    private readonly file: File;
    private nativeSource: native.ft.FileTransferSource;

    constructor(file: File) {
        super();
        this.file = file;
    }


    async fileSize(): Promise<number> {
        return this.file.size;
    }

    getNativeSource() {
        if(this.nativeSource) return this.nativeSource;

        this.nativeSource = native.ft.upload_transfer_object_from_file(path.dirname(this.file.path), path.basename(this.file.path));
        return this.nativeSource;
    }

    getFile(): File {
        return this.file;
    }
}

class ResponseTransferTargetImpl extends ResponseTransferTarget {
    nativeTarget: native.ft.FileTransferTarget;
    buffer: Uint8Array;
    private response: Response;

    constructor() {
        super();
    }

    initialize(bufferSize: number) {
        this.buffer = new Uint8Array(bufferSize);
        this.nativeTarget = native.ft.download_transfer_object_from_buffer(this.buffer.buffer);
    }

    getResponse(): Response {
        return this.response;
    }

    hasResponse(): boolean {
        return typeof this.response === "object";
    }

    createResponseFromBuffer() {
        const buffer = this.buffer.buffer.slice(this.buffer.byteOffset, this.buffer.byteOffset + Math.min(64, this.buffer.byteLength));
        this.response = new Response(this.buffer, {
            status: 200,
            statusText: "success",
            headers: {
                "X-media-bytes": base64_encode_ab(buffer)
            }
        });
    }
}

class FileTransferTargetImpl extends FileTransferTarget {
    private path: string;
    private name: string;

    constructor(path: string, name?: string) {
        super();

        this.path = path;
        this.name = name;
    }

    async requestPath() {
        if(typeof this.path === "string") {
            return;
        }

        const result = await electron.remote.dialog.showSaveDialog({ defaultPath: this.name });
        if(result.canceled) {
            throw tr("download canceled");
        }

        if(!result.filePath) {
            throw tr("invalid result path");
        }

        this.path = path.dirname(result.filePath);
        this.name = path.basename(result.filePath);
    }

    getNativeTarget(fallbackName: string, expectedSize: number) {
        this.name = this.name || fallbackName;

        return native.ft.download_transfer_object_from_file(this.path, this.name, expectedSize);
    }

    getFilePath(): string {
        return this.path;
    }

    getFileName(): string {
        return this.name;
    }

    hasFileName(): boolean {
        return typeof this.name === "string";
    }
}