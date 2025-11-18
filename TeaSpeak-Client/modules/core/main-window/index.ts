import {BrowserWindow, app, dialog, MessageBoxOptions} from "electron";
import * as path from "path";

export let is_debug: boolean;
export let allow_dev_tools: boolean;

import {Arguments, processArguments} from "../../shared/process-arguments";
import {
    getLoaderWindow,
    hideAppLoaderWindow,
    setAppLoaderStatus,
    showAppLoaderWindow
} from "../windows/app-loader/controller/AppLoader";
import {loadUiPack} from "../ui-loader/Loader";
import {loadLocalUiCache} from "../ui-loader/Cache";
import {closeMainWindow, showMainWindow} from "../windows/main-window/controller/MainWindow";
import {showUpdateWindow} from "../windows/client-updater/controller/ClientUpdate";
import {
    currentClientVersion,
    availableClientUpdate,
    setClientUpdateChannel,
    initializeAppUpdater
} from "../app-updater";
import * as app_updater from "../app-updater";

export async function execute() {
    console.log("Main app executed!");

    is_debug = processArguments.has_flag(...Arguments.DEBUG);
    allow_dev_tools = processArguments.has_flag(...Arguments.DEV_TOOLS);
    if(is_debug) {
        console.log("Enabled debug!");
        console.log("Arguments: %o", processArguments);
    }

    setAppLoaderStatus("Bootstrapping", 0);
    await showAppLoaderWindow();
    await initializeAppUpdater();

    /* TODO: Remove this (currently required somewhere within the renderer) */
    const version = await app_updater.currentClientVersion();
    global["app_version_client"] = version.toString();

    setAppLoaderStatus("Checking for updates", .1);
    try {
        if(processArguments.has_value(Arguments.UPDATER_CHANNEL)) {
            setClientUpdateChannel(processArguments.value(Arguments.UPDATER_CHANNEL));
        }

        const newVersion = await availableClientUpdate();
        if(newVersion) {
            setAppLoaderStatus("Awaiting update", .15);

            const result = await dialog.showMessageBox(getLoaderWindow(), {
                buttons: ["Update now", "No thanks"],
                title: "Update available!",
                message:
                    "There is an update available!\n" +
                    "Should we update now?\n" +
                    "\n" +
                    "Current version: " + (await currentClientVersion()).toString() + "\n" +
                    "Target version: " + newVersion.version.toString(true)
            } as MessageBoxOptions);

            if(result.response === 0) {
                /* TODO: Execute update! */
                await showUpdateWindow();
                hideAppLoaderWindow();
                return;
            }
        }
    } catch (error) {
        console.warn("Failed to check for a client update: %o", error);
    }
    setAppLoaderStatus("Initialize backend", .2);

    console.log("Setting up render backend");
    require("../render-backend");

    let uiEntryPoint;
    try {
        setAppLoaderStatus("Loading ui cache", .25);
        await loadLocalUiCache();
        uiEntryPoint = await loadUiPack((message, index) => {
            setAppLoaderStatus(message, index * .75 + .25);
        });
    } catch (error) {
        hideAppLoaderWindow();
        console.error("Failed to load ui: %o", error);

        closeMainWindow(true);
        await dialog.showMessageBox({
            type: "error",
            buttons: ["exit"],
            title: "A critical error happened while loading TeaClient!",
            message: (error || "no error").toString()
        });
        return;
    }

    if(!uiEntryPoint) {
        throw "missing ui entry point";
    }

    setAppLoaderStatus("Starting client", 1);
    await showMainWindow(uiEntryPoint);
    hideAppLoaderWindow();
}
