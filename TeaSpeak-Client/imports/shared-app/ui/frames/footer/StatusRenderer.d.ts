import { Registry } from "tc-shared/events";
import { ConnectionStatusEvents } from "tc-shared/ui/frames/footer/StatusDefinitions";
import * as React from "react";
export declare const StatusEvents: React.Context<Registry<ConnectionStatusEvents>>;
export declare const StatusTextRenderer: React.MemoExoticComponent<() => JSX.Element>;
export declare const StatusDetailRenderer: () => JSX.Element;
