/// <reference types="react" />
import { Registry } from "tc-shared/events";
import { ConnectionListUIEvents } from "tc-shared/ui/frames/connection-handler-list/Definitions";
export declare const ConnectionHandlerList: (props: {
    events: Registry<ConnectionListUIEvents>;
}) => JSX.Element;
