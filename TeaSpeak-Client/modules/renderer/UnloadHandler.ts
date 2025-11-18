import {Settings, settings} from "tc-shared/settings";
import {tr} from "tc-shared/i18n/localize";
import {Arguments, processArguments} from "../shared/process-arguments";
import {remote} from "electron";
import {server_connections} from "tc-shared/ConnectionManager";

window.onbeforeunload = event => {
    if(settings.getValue(Settings.KEY_DISABLE_UNLOAD_DIALOG))
        return;

    const active_connections = server_connections.getAllConnectionHandlers().filter(e => e.connected);
    if(active_connections.length == 0) return;

    const do_exit = (closeWindow: boolean) => {
        const dp = server_connections.getAllConnectionHandlers().map(e => {
            if(e.serverConnection.connected())
                return e.serverConnection.disconnect(tr("client closed"))
                    .catch(error => {
                        console.warn(tr("Failed to disconnect from server %s on client close: %o"),
                            e.serverConnection.remote_address().host + ":" + e.serverConnection.remote_address().port,
                            error
                        );
                    });
            return Promise.resolve();
        });

        if(closeWindow) {
            const exit = () => {
                const {remote} = window.require('electron');
                remote.getCurrentWindow().close();
            };

            Promise.all(dp).then(exit);
            /* force exit after 2500ms */
            setTimeout(exit, 2500);
        }
    };

    if(processArguments.has_flag(Arguments.DEBUG)) {
        do_exit(false);
        return;
    }

    remote.dialog.showMessageBox(remote.getCurrentWindow(), {
        type: 'question',
        buttons: ['Yes', 'No'],
        title: 'Confirm',
        message: 'Are you really sure?\nYou\'re still connected!'
    }).then(result => {
        if(result.response === 0) {
            /* prevent quitting because we try to disconnect */
            window.onbeforeunload = e => e.preventDefault();
            do_exit(true);
        }
    });


    event.preventDefault();
    event.returnValue = "question";
}