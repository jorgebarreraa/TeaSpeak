/// <reference types="react" />
import { Registry } from "tc-shared/events";
import { ControlBarEvents } from "tc-shared/ui/frames/control-bar/Definitions";
import { ConnectionListUIEvents } from "tc-shared/ui/frames/connection-handler-list/Definitions";
import { SideBarEvents } from "tc-shared/ui/frames/SideBarDefinitions";
import { SideHeaderEvents } from "tc-shared/ui/frames/side/HeaderDefinitions";
import { ServerEventLogUiEvents } from "tc-shared/ui/frames/log/Definitions";
import { HostBannerUiEvents } from "tc-shared/ui/frames/HostBannerDefinitions";
import { AppUiEvents } from "tc-shared/ui/AppDefinitions";
export declare const TeaAppMainView: (props: {
    events: Registry<AppUiEvents>;
    controlBar: Registry<ControlBarEvents>;
    connectionList: Registry<ConnectionListUIEvents>;
    sidebar: Registry<SideBarEvents>;
    sidebarHeader: Registry<SideHeaderEvents>;
    log: Registry<ServerEventLogUiEvents>;
    hostBanner: Registry<HostBannerUiEvents>;
}) => JSX.Element;
