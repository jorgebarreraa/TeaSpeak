import { ConnectionHandler } from "../../../ConnectionHandler";
import { Registry } from "tc-events";
import { TransferInfoEvents } from "tc-shared/ui/modal/transfer/FileTransferInfoDefinitions";
export declare const initializeTransferInfoController: (connection: ConnectionHandler, events: Registry<TransferInfoEvents>) => void;
