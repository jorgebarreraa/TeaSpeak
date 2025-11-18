import { ChannelDescriptionUiEvents } from "tc-shared/ui/frames/side/ChannelDescriptionDefinitions";
import { Registry } from "tc-shared/events";
import * as React from "react";
export declare const ChannelDescriptionRenderer: React.MemoExoticComponent<(props: {
    events: Registry<ChannelDescriptionUiEvents>;
}) => JSX.Element>;
