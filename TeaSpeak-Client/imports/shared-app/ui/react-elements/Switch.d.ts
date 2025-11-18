import * as React from "react";
export interface SwitchProperties {
    value?: boolean;
    initialState?: boolean;
    className?: string;
    label?: string | React.ReactElement;
    labelSide?: "right" | "left";
    disabled?: boolean;
    onChange?: (value: boolean) => void;
    onBlur?: () => void;
}
export interface SwitchState {
    checked: boolean;
    disabled?: boolean;
}
export declare class Switch extends React.Component<SwitchProperties, SwitchState> {
    private readonly ref;
    constructor(props: any);
    render(): JSX.Element;
    focus(): void;
}
