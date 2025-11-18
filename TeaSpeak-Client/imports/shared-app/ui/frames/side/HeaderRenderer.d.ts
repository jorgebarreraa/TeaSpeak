import * as React from "react";
import { Registry } from "tc-shared/events";
import { SideHeaderEvents, SideHeaderState } from "tc-shared/ui/frames/side/HeaderDefinitions";
export declare const SideHeaderRenderer: React.MemoExoticComponent<(props: {
    events: Registry<SideHeaderEvents>;
    state: SideHeaderState;
}) => JSX.Element>;
