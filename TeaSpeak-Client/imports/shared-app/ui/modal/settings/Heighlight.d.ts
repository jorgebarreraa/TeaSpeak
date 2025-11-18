import * as React from "react";
export declare const HighlightContainer: (props: {
    children: React.ReactNode | React.ReactNode[];
    classList?: string;
    highlightedId?: string;
    onClick?: () => void;
}) => JSX.Element;
export declare const HighlightRegion: (props: React.HTMLProps<HTMLDivElement> & {
    highlightId: string;
}) => React.DetailedReactHTMLElement<React.HTMLProps<HTMLDivElement> & {
    highlightId: string;
}, HTMLDivElement>;
export declare const HighlightText: (props: {
    highlightId: string;
    className?: string;
    children?: React.ReactNode | React.ReactNode[];
}) => JSX.Element;
