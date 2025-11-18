import {createErrorModal} from "tc-shared/ui/elements/Modal";

import * as electron from "electron";
import {tr, tra} from "tc-shared/i18n/localize";
import {handleNativeConnectRequest} from "tc-shared/main";

electron.ipcRenderer.on('connect', (event, url) => handle_native_connect_request(url));

function handle_native_connect_request(urlString: string) {
    console.log(tr("Received connect event to %s"), urlString);


    if(!urlString.toLowerCase().startsWith("teaclient://")) {
        createErrorModal(tr("Failed to parse connect URL"), tra("Failed to parse connect URL (Unknown protocol).{:br:}URL: {}", urlString)).open();
        return;
    }

    let url: URL;
    try {
        url = new URL("https://" + urlString.substring(10));
    } catch(error) {
        createErrorModal(tr("Failed to parse connect URL"), tra("Failed to parse connect URL.{:br:}URL: {}", urlString)).open();
        return;
    }

    handleNativeConnectRequest(url);
}