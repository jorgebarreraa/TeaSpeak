import { ConnectionHandler } from "../../../ConnectionHandler";
import { Registry } from "tc-events";
import { FileBrowserEvents } from "tc-shared/ui/modal/transfer/FileDefinitions";
export declare function initializeRemoteFileBrowserController(connection: ConnectionHandler, events: Registry<FileBrowserEvents>): void;
