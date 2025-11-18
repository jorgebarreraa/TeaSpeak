import * as React from "react";
import { Options } from "./parser";
import ReactRenderer from "./renderer/react";
export declare function XBBCodeRenderer(props: {
    children: string;
    options?: Options;
    renderer?: ReactRenderer;
}): React.ReactElement;
export declare function XBBCodeRenderer(props: {
    text: string;
    children: never;
    options?: Options;
    renderer?: ReactRenderer;
}): React.ReactElement;
