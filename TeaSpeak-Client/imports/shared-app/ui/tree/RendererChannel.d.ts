import * as React from "react";
import { RDPChannel } from "tc-shared/ui/tree/RendererDataProvider";
export declare class ChannelIconClass extends React.Component<{
    channel: RDPChannel;
}, {}> {
    render(): JSX.Element;
}
export declare class ChannelIconsRenderer extends React.Component<{
    channel: RDPChannel;
}, {}> {
    render(): JSX.Element;
}
export declare class RendererChannel extends React.Component<{
    channel: RDPChannel;
}, {}> {
    render(): JSX.Element;
    componentDidUpdate(prevProps: Readonly<{
        channel: RDPChannel;
    }>, prevState: Readonly<{}>, snapshot?: any): void;
    componentDidMount(): void;
    private fixCssVariables;
}
