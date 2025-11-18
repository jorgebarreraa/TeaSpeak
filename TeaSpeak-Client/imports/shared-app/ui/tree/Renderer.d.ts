/// <reference types="react" />
import { Registry } from "tc-shared/events";
import { ChannelTreeUIEvents } from "tc-shared/ui/tree/Definitions";
export declare const ChannelTreeRenderer: (props: {
    handlerId: string;
    events: Registry<ChannelTreeUIEvents>;
}) => JSX.Element;
