import * as React from "react";
import { ReactElement } from "react";
export interface ProgressBarState {
    value?: number;
    text?: ReactElement | string;
    type?: "normal" | "error" | "success";
}
export interface ProgressBarProperties {
    value: number;
    text?: ReactElement | string;
    type: "normal" | "error" | "success";
    className?: string;
}
export declare class ProgressBar extends React.Component<ProgressBarProperties, ProgressBarState> {
    constructor(props: any);
    render(): JSX.Element;
}
