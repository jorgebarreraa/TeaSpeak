import * as React from "react";
import { ReactNode } from "react";
export interface TooltipState {
    forceShow: boolean;
    hovered: boolean;
    pageX: number;
    pageY: number;
}
export interface TooltipProperties {
    tooltip: () => ReactNode | ReactNode[] | string;
    className?: string;
    /**
     * Enable the tooltip already when the span is hovered
     */
    spawnHover?: boolean;
}
export declare class Tooltip extends React.PureComponent<TooltipProperties, TooltipState> {
    readonly tooltipId: string;
    private refContainer;
    private currentContainer;
    constructor(props: any);
    componentWillUnmount(): void;
    render(): JSX.Element;
    componentDidUpdate(prevProps: Readonly<TooltipProperties>, prevState: Readonly<TooltipState>, snapshot?: any): void;
    private onMouseEnter;
    updatePosition(): void;
}
export declare const IconTooltip: (props: {
    children?: React.ReactNode | React.ReactNode[];
    className?: string;
    outerClassName?: string;
}) => JSX.Element;
export declare const TooltipHook: React.MemoExoticComponent<() => JSX.Element>;
