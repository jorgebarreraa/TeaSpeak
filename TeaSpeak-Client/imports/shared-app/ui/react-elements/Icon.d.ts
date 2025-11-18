import * as React from "react";
import { RemoteIcon, RemoteIconInfo } from "tc-shared/file/Icons";
export declare const IconRenderer: (props: {
    icon: string;
    title?: string;
    className?: string;
}) => JSX.Element;
export declare const RemoteIconRenderer: (props: {
    icon: RemoteIcon | undefined;
    className?: string;
    title?: string;
}) => JSX.Element;
export declare const RemoteIconInfoRenderer: React.MemoExoticComponent<(props: {
    icon: RemoteIconInfo | undefined;
    className?: string;
    title?: string;
}) => JSX.Element>;
