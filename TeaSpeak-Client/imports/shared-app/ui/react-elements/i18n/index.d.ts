import * as React from "react";
export declare class Translatable extends React.Component<{
    children: string | (string | React.ReactElement<HTMLBRElement>)[];
    trIgnore?: boolean;
}, {
    translated: string;
}> {
    protected renderElementIndex: number;
    constructor(props: any);
    render(): any[];
    componentDidMount(): void;
    componentWillUnmount(): void;
}
export declare type VariadicTranslatableChild = React.ReactElement | string | number;
export declare const VariadicTranslatable: (props: {
    text: string;
    children?: VariadicTranslatableChild[] | VariadicTranslatableChild;
}) => JSX.Element;
declare global {
    interface Window {
        i18nInstances: Translatable[];
    }
}
