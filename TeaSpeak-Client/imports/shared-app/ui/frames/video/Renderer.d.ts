import * as React from "react";
import { Registry } from "tc-shared/events";
import { ChannelVideoEvents } from "./Definitions";
export declare const VideoIdContext: React.Context<string>;
export declare const RendererVideoEventContext: React.Context<Registry<ChannelVideoEvents>>;
export declare const VideoContainer: React.MemoExoticComponent<(props: {
    isSpotlight: boolean;
}) => JSX.Element>;
export declare const ChannelVideoRenderer: (props: {
    handlerId: string;
    events: Registry<ChannelVideoEvents>;
}) => JSX.Element;
