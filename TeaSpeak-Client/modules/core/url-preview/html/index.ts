import * as electron from "electron";
import * as path from "path";

interface Options {
    showBackButton: boolean,
    showForwardButton: boolean,
    showReloadButton: boolean,
    showUrlBar: boolean,
    showAddTabButton: boolean,
    closableTabs: boolean,
    verticalTabs: boolean,
    defaultFavicons: boolean,
    newTabCallback: (url: string, options: any) => any,
    changeTabCallback: () => any,
    newTabParams: any
}

interface NewTabOptions {
    id: string,
    node: boolean,
    readonlyUrl: boolean,
    contextMenu: boolean,
    webviewAttributes: any,
    icon: "clean" | "default" | string,
    title: "default",
    close: boolean
}

const enav = new (require('./navigation'))({
    closableTabs: true,
    showAddTabButton: false,
    defaultFavicons: false,

    changeTabCallback: new_tab => {
        if(new_tab === undefined)
            window.close();
    }
} as Options);

/* Required here: https://github.com/simply-coded/electron-navigation/blob/master/index.js#L364 */
enav.executeJavaScript = () => {}; /* just to suppress an error cause by the API */

let _id_counter = 0;
const execute_preview = (url: string) => {
    const id = "preview_" + (++_id_counter);
    const tab: HTMLElement & { executeJavaScript(js: string) : Promise<any> } = enav.newTab(url, {
        id: id,
        contextMenu: false,
        readonlyUrl: true,
        icon: "clean",
        webviewAttributes: {
            'preload': path.join(__dirname, "inject.js")
        }
    } as NewTabOptions);

    /* we only want to preload our script once */
    const show_preview = () => {
        tab.removeEventListener("dom-ready", show_preview);
        tab.removeAttribute("preload");

        tab.executeJavaScript('__teaclient_preview_notice()').catch((error) => console.log("Failed to show TeaClient overlay! Error: %o", error));
    };

    tab.addEventListener("dom-ready", show_preview);

    tab.addEventListener('did-fail-load', (res: any) => {
        console.error("Side load failed: %o", res);
        if (res.errorCode != -3) {
            res.target.executeJavaScript('__teaclient_preview_error("' + res.errorCode + '", "' + encodeURIComponent(res.errorDescription) + '", "' + encodeURIComponent(res.validatedURL) + '")').catch(error => {
                console.warn("Failed to show error page: %o", error);
            });
        }
    });

    tab.addEventListener('close', () => enav.closeTab(id));
};

electron.ipcRenderer.on('preview', (event, url) => execute_preview(url));