import * as React from "react";
export interface ContextDividerProperties {
    id: string;
    direction: "vertical" | "horizontal";
    defaultValue: number;
    separatorClassName?: string;
    separatorActiveClassName?: string;
    children?: never;
}
export interface ContextDividerState {
    active: boolean;
}
export declare class ContextDivider extends React.Component<ContextDividerProperties, ContextDividerState> {
    private readonly refSeparator;
    private readonly listenerMove;
    private readonly listenerUp;
    private value;
    constructor(props: any);
    render(): JSX.Element;
    componentDidMount(): void;
    componentWillUnmount(): void;
    private startMovement;
    private stopMovement;
    private applySeparator;
}
