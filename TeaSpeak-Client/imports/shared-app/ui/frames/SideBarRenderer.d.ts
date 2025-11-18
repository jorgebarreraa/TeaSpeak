/// <reference types="react" />
import { SideHeaderEvents } from "tc-shared/ui/frames/side/HeaderDefinitions";
import { Registry } from "tc-shared/events";
import { SideBarEvents } from "tc-shared/ui/frames/SideBarDefinitions";
export declare const SideBarRenderer: (props: {
    events: Registry<SideBarEvents>;
    eventsHeader: Registry<SideHeaderEvents>;
    className?: string;
}) => JSX.Element;
