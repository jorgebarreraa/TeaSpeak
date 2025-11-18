import * as React from "react";
export declare const RadioButton: (props: {
    children?: React.ReactNode | string | React.ReactNode[];
    name: string;
    selected: boolean;
    disabled?: boolean;
    onChange: (checked: boolean) => void;
}) => JSX.Element;
