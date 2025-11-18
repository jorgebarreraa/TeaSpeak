import { AbstractModal } from "tc-shared/ui/react-elements/modal/Definitions";
import React from "react";
export declare class ModalFrameTopRenderer extends React.PureComponent<{
    modalInstance: AbstractModal;
    className?: string;
    onClose?: () => void;
    onPopout?: () => void;
    onMinimize?: () => void;
    replacePageTitle: boolean;
}> {
    private readonly refTitle;
    private titleElement;
    private observer;
    componentDidMount(): void;
    componentWillUnmount(): void;
    render(): JSX.Element;
    private updatePageTitle;
}
export declare class ModalBodyRenderer extends React.PureComponent<{
    modalInstance: AbstractModal;
    className?: string;
}> {
    constructor(props: any);
    render(): JSX.Element;
}
export declare class ModalFrameRenderer extends React.PureComponent<{
    windowed: boolean;
    children: [React.ReactElement<ModalFrameTopRenderer>, React.ReactElement<ModalBodyRenderer>];
}> {
    render(): JSX.Element;
}
export declare class PageModalRenderer extends React.PureComponent<{
    modalInstance: AbstractModal;
    onBackdropClicked: () => void;
    children: React.ReactElement<ModalFrameRenderer>;
    shown: boolean;
}> {
    constructor(props: any);
    render(): JSX.Element;
}
export declare const WindowModalRenderer: (props: {
    children: [React.ReactElement<ModalFrameTopRenderer>, React.ReactElement<ModalBodyRenderer>];
}) => JSX.Element;
