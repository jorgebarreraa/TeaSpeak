/// <reference types="react" />
import { Registry } from "tc-shared/events";
import { ChannelBarUiEvents } from "tc-shared/ui/frames/side/ChannelBarDefinitions";
export declare const ChannelBarRenderer: (props: {
    events: Registry<ChannelBarUiEvents>;
}) => JSX.Element;
