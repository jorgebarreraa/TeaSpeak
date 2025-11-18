import * as React from "react";
export interface ButtonProperties {
    color?: "green" | "blue" | "red" | "purple" | "brown" | "yellow" | "default" | "none";
    type?: "normal" | "small" | "extra-small";
    className?: string;
    onClick?: (event: React.MouseEvent) => void;
    hidden?: boolean;
    disabled?: boolean;
    title?: string;
    transparency?: boolean;
}
export interface ButtonState {
    disabled?: boolean;
}
export declare class Button extends React.Component<ButtonProperties, ButtonState> {
    constructor(props: any);
    render(): JSX.Element;
}
