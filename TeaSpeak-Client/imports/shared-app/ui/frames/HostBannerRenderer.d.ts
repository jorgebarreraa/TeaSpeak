import { Registry } from "tc-shared/events";
import { HostBannerInfoSet, HostBannerUiEvents } from "tc-shared/ui/frames/HostBannerDefinitions";
import * as React from "react";
export declare const HostBannerRenderer: React.MemoExoticComponent<(props: {
    banner: HostBannerInfoSet;
    clickable: boolean;
    className?: string;
}) => JSX.Element>;
export declare const HostBanner: React.MemoExoticComponent<(props: {
    events: Registry<HostBannerUiEvents>;
}) => JSX.Element>;
