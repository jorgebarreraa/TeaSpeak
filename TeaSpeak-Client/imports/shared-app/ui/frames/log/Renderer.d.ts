/// <reference types="react" />
import { Registry } from "tc-shared/events";
import { ServerEventLogUiEvents } from "tc-shared/ui/frames/log/Definitions";
export declare const ServerLogFrame: (props: {
    events: Registry<ServerEventLogUiEvents>;
}) => JSX.Element;
