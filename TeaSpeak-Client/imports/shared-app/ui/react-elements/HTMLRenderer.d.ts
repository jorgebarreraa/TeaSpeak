import * as React from "react";
export declare class HTMLRenderer extends React.PureComponent<{
    purify: boolean;
    children: string;
}, {}> {
    private readonly reference;
    private readonly newNodes;
    private originalNode;
    constructor(props: any);
    render(): JSX.Element;
    componentDidMount(): void;
    componentWillUnmount(): void;
}
