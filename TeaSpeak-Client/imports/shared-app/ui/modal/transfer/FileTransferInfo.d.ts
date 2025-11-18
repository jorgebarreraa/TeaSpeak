/// <reference types="react" />
import { Registry } from "tc-shared/events";
import { TransferInfoEvents } from "tc-shared/ui/modal/transfer/FileTransferInfoDefinitions";
export declare const FileTransferInfo: (props: {
    events: Registry<TransferInfoEvents>;
}) => JSX.Element;
