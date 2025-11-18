import * as React from "react";
import { RDPClient } from "tc-shared/ui/tree/RendererDataProvider";
export declare class ClientStatus extends React.Component<{
    client: RDPClient;
}, {}> {
    render(): JSX.Element;
}
export declare class ClientName extends React.Component<{
    client: RDPClient;
}, {}> {
    render(): JSX.Element;
}
export declare class ClientTalkStatusIcon extends React.Component<{
    client: RDPClient;
}, {}> {
    render(): JSX.Element;
}
export declare class ClientIconsRenderer extends React.Component<{
    client: RDPClient;
}, {}> {
    render(): JSX.Element;
}
declare global {
    interface HTMLElement {
        createTextRange: any;
    }
}
export declare class RendererClient extends React.Component<{
    client: RDPClient;
}, {}> {
    render(): JSX.Element;
}
