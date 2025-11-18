import * as React from "react";
import { ReactElement } from "react";
export interface CheckboxProperties {
    label?: ReactElement | string;
    disabled?: boolean;
    onChange?: (value: boolean) => void;
    value?: boolean;
    initialValue?: boolean;
    className?: string;
    children?: never;
}
export interface CheckboxState {
    checked?: boolean;
    disabled?: boolean;
}
export declare class Checkbox extends React.Component<CheckboxProperties, CheckboxState> {
    constructor(props: any);
    render(): JSX.Element;
    private onStateChange;
}
