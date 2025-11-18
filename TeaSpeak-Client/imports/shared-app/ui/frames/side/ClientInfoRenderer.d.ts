/// <reference types="react" />
import { ClientInfoEvents } from "tc-shared/ui/frames/side/ClientInfoDefinitions";
import { Registry } from "tc-shared/events";
export declare const ClientInfoRenderer: (props: {
    events: Registry<ClientInfoEvents>;
}) => JSX.Element;
