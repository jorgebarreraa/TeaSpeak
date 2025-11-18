import * as React from "react";
import { ReactElement } from "react";
export declare const ControlledBoxedInputField: (props: {
    prefix?: string;
    suffix?: string;
    placeholder?: string;
    disabled?: boolean;
    editable?: boolean;
    value?: string;
    rightIcon?: () => ReactElement;
    leftIcon?: () => ReactElement;
    inputBox?: () => ReactElement;
    isInvalid?: boolean;
    className?: string;
    maxLength?: number;
    size?: "normal" | "large" | "small";
    type?: "text" | "password" | "number";
    onChange: (newValue?: string) => void;
    onEnter?: () => void;
    onFocus?: () => void;
    onBlur?: () => void;
    finishOnEnter?: boolean;
    refInput?: React.RefObject<HTMLInputElement>;
}) => JSX.Element;
export interface BoxedInputFieldProperties {
    prefix?: string;
    suffix?: string;
    placeholder?: string;
    disabled?: boolean;
    editable?: boolean;
    value?: string;
    defaultValue?: string;
    rightIcon?: () => ReactElement;
    leftIcon?: () => ReactElement;
    inputBox?: () => ReactElement;
    isInvalid?: boolean;
    className?: string;
    maxLength?: number;
    size?: "normal" | "large" | "small";
    type?: "text" | "password" | "number";
    onFocus?: (event: React.FocusEvent | React.MouseEvent) => void;
    onBlur?: () => void;
    onChange?: (newValue: string) => void;
    onInput?: (newValue: string) => void;
    finishOnEnter?: boolean;
}
export interface BoxedInputFieldState {
    disabled?: boolean;
    defaultValue?: string;
    isInvalid?: boolean;
    value?: string;
}
export declare class BoxedInputField extends React.Component<BoxedInputFieldProperties, BoxedInputFieldState> {
    private refInput;
    private inputEdited;
    constructor(props: any);
    render(): JSX.Element;
    focusInput(): void;
    private onKeyDown;
    private onInputBlur;
}
export declare const ControlledFlatInputField: (props: {
    type?: "text" | "password" | "number";
    value: string;
    placeholder?: string;
    className?: string;
    label?: React.ReactNode;
    labelType?: "static" | "floating";
    labelClassName?: string;
    labelFloatingClassName?: string;
    help?: React.ReactNode;
    helpClassName?: string;
    invalid?: React.ReactNode;
    invalidClassName?: string;
    disabled?: boolean;
    editable?: boolean;
    onFocus?: () => void;
    onBlur?: () => void;
    onChange?: (newValue?: string) => void;
    onInput?: (newValue?: string) => void;
    onEnter?: () => void;
    finishOnEnter?: boolean;
}) => JSX.Element;
export interface FlatInputFieldProperties {
    defaultValue?: string;
    value?: string;
    placeholder?: string;
    className?: string;
    label?: string | React.ReactElement;
    labelType?: "static" | "floating";
    labelClassName?: string;
    labelFloatingClassName?: string;
    type?: "text" | "password" | "number";
    help?: string | React.ReactElement;
    helpClassName?: string;
    invalidClassName?: string;
    disabled?: boolean;
    editable?: boolean;
    onFocus?: () => void;
    onBlur?: () => void;
    onChange?: (newValue?: string) => void;
    onInput?: (newValue?: string) => void;
    onEnter?: () => void;
    finishOnEnter?: boolean;
}
export interface FlatInputFieldState {
    filled: boolean;
    placeholder?: string;
    disabled?: boolean;
    editable?: boolean;
    isInvalid: boolean;
    invalidMessage: string | React.ReactElement;
}
export declare class FlatInputField extends React.Component<FlatInputFieldProperties, FlatInputFieldState> {
    private readonly refInput;
    constructor(props: any);
    render(): JSX.Element;
    private onChange;
    value(): string;
    setValue(value: string | undefined): void;
    inputElement(): HTMLInputElement | undefined;
    focus(): void;
}
export declare const ControlledSelect: (props: {
    type?: "flat" | "boxed";
    className?: string;
    value: string;
    placeHolder?: string;
    label?: React.ReactNode;
    labelClassName?: string;
    help?: React.ReactNode;
    helpClassName?: string;
    invalid?: React.ReactNode;
    invalidClassName?: string;
    disabled?: boolean;
    onFocus?: () => void;
    onBlur?: () => void;
    onChange?: (event?: React.ChangeEvent<HTMLSelectElement>) => void;
    children: React.ReactElement<HTMLOptionElement | HTMLOptGroupElement> | React.ReactElement<HTMLOptionElement | HTMLOptGroupElement>[];
}) => JSX.Element;
export interface SelectProperties {
    type?: "flat" | "boxed";
    refSelect?: React.RefObject<HTMLSelectElement>;
    defaultValue?: string;
    value?: string;
    className?: string;
    label?: string | React.ReactElement;
    labelClassName?: string;
    help?: string | React.ReactElement;
    helpClassName?: string;
    invalidClassName?: string;
    disabled?: boolean;
    editable?: boolean;
    title?: string;
    onFocus?: () => void;
    onBlur?: () => void;
    onChange?: (event?: React.ChangeEvent<HTMLSelectElement>) => void;
}
export interface SelectFieldState {
    disabled?: boolean;
    isInvalid: boolean;
    invalidMessage: string | React.ReactElement;
}
export declare class Select extends React.Component<SelectProperties, SelectFieldState> {
    private refSelect;
    constructor(props: any);
    render(): JSX.Element;
    selectElement(): HTMLSelectElement | undefined;
    focus(): void;
}
