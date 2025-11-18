import * as React from "react";
export declare class TabEntry extends React.Component<{
    children: [React.ReactNode, React.ReactNode];
    id: string;
}, {}> {
    constructor(props: any);
    render(): React.ReactNode;
}
export declare class Tab extends React.PureComponent<{
    children: React.ReactElement[];
    defaultTab: string;
    selectedTab?: string;
    permanentRender?: boolean;
    className?: string;
}, {
    selectedTab: string;
}> {
    constructor(props: any);
    render(): JSX.Element;
}
