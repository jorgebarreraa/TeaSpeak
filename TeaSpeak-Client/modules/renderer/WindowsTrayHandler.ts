import * as loader from "tc-loader";
import {Stage} from "tc-loader";

import {MenuItemConstructorOptions, NativeImage, remote, Tray} from "electron";
import {clientIconClassToImage} from "./IconHelper";
import {ClientIcon} from "svg-sprites/client-icons";
import {ConnectionHandler, ConnectionState} from "tc-shared/ConnectionHandler";
import {server_connections} from "tc-shared/ConnectionManager";
import {tr} from "tc-shared/i18n/localize";
import {global_client_actions} from "tc-shared/events/GlobalEvents";

const kTrayGlobalUniqueId = "9ccaf91c-a54f-45e0-b061-c50c9f7864ca";

let tray: Tray;
let eventListener = [];
let defaultIcon: NativeImage;
async function initializeTray() {
    defaultIcon = clientIconClassToImage(ClientIcon.TeaspeakLogo);
    defaultIcon = remote.nativeImage.createFromBuffer(defaultIcon.toPNG());

    tray = new remote.Tray(defaultIcon);
    tray.setTitle("TeaSpeak - Client");
    tray.on("double-click", () => remote.getCurrentWindow().show());

    server_connections.events().on("notify_active_handler_changed", event => initializeConnection(event.newHandler));
    initializeConnection(undefined);
}

function initializeConnection(connection: ConnectionHandler) {
    eventListener.forEach(callback => callback());
    eventListener = [];

    let showClientStatus = connection?.connection_state === ConnectionState.CONNECTED;
    let clientStatusIcon: ClientIcon = connection?.getClient().getStatusIcon();

    const updateTray = () => {
        if(showClientStatus) {
            let icon = clientIconClassToImage(clientStatusIcon);
            icon = remote.nativeImage.createFromBuffer(icon.toPNG());
            tray.setImage(icon);
            tray.setToolTip("TeaSpeak - Client\nConnected to " + connection.channelTree.server.properties.virtualserver_name);
        } else {
            tray.setImage(defaultIcon);
            tray.setToolTip("TeaSpeak - Client");
        }
    }

    const updateContextMenu = () => {
        let items: MenuItemConstructorOptions[] = [];

        items.push(
            { label: tr("Show TeaClient"), type: "normal", icon: defaultIcon, click: () => remote.getCurrentWindow().show() },
            { label: "seperator", type: "separator" },
        );

        items.push(
            {
                label: tr("Connect to server"),
                type: "normal",
                icon: clientIconClassToImage(ClientIcon.Connect),
                click: () => {
                    global_client_actions.fire("action_open_window_connect", { newTab: connection?.connected });
                    remote.getCurrentWindow().show();
                }
            },
            {
                label: tr("Disconnect from current server"),
                type: "normal",
                icon: clientIconClassToImage(ClientIcon.Disconnect),
                click: () => connection.disconnectFromServer(),
                enabled: connection?.connected
            },
            { label: "seperator", type: "separator" },
        )

        if(connection) {
            if(connection.isMicrophoneDisabled()) {
                items.push({
                    label: tr("Enable microphone"),
                    type: "normal",
                    icon: clientIconClassToImage(ClientIcon.ActivateMicrophone),
                    checked: true,
                    click: () => connection.setMicrophoneMuted(false)
                });
            } else if(connection.isMicrophoneMuted()) {
                items.push({
                    label: tr("Unmute microphone"),
                    type: "normal",
                    icon: clientIconClassToImage(ClientIcon.InputMuted),
                    checked: true,
                    click: () => {
                        connection.setMicrophoneMuted(false);
                        connection.acquireInputHardware().then(() => {});
                    }
                });
            } else {
                items.push({
                    label: tr("Mute microphone"),
                    type: "normal",
                    icon: clientIconClassToImage(ClientIcon.InputMuted),
                    checked: false,
                    click: () => connection.setMicrophoneMuted(true)
                });
            }

            if(connection.isSpeakerMuted()) {
                items.push({
                    label: tr("Unmute speaker/headphones"),
                    type: "normal",
                    icon: clientIconClassToImage(ClientIcon.OutputMuted),
                    checked: true,
                    click: () => connection.setSpeakerMuted(false)
                });
            } else {
                items.push({
                    label: tr("Mute speaker/headphones"),
                    type: "normal",
                    icon: clientIconClassToImage(ClientIcon.OutputMuted),
                    checked: true,
                    click: () => connection.setSpeakerMuted(false)
                });
            }

            items.push(
                { label: "seperator", type: "separator" }
            );
        }

        items.push(
            { label: tr("Quit"), type: "normal", icon: clientIconClassToImage(ClientIcon.CloseButton), click: () => remote.getCurrentWindow().close() }
        );

        tray.setContextMenu(remote.Menu.buildFromTemplate(items));
    };

    if(connection) {
        eventListener.push(connection.channelTree.server.events.on("notify_properties_updated", event => {
            if("virtualserver_name" in event.updated_properties) {
                updateTray();
            }
        }));

        eventListener.push(connection.events().on("notify_connection_state_changed", event => {
            showClientStatus = event.newState === ConnectionState.CONNECTED;
            updateTray();
            updateContextMenu();
        }));

        eventListener.push(connection.getClient().events.on("notify_status_icon_changed", event => {
            clientStatusIcon = event.newIcon;
            updateTray();
        }));

        eventListener.push(connection.events().on("notify_state_updated", event => {
            switch (event.state) {
                case "away":
                case "microphone":
                case "speaker":
                    updateContextMenu();
                    break;
            }
        }));
    }

    updateContextMenu();
    updateTray();
}

loader.register_task(Stage.JAVASCRIPT_INITIALIZING, {
    name: "tray bar",
    function: initializeTray,
    priority: 10
});

window.addEventListener("unload", () => tray.destroy());