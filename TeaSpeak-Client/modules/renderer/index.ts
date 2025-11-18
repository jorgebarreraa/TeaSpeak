/* --------------- bootstrap  --------------- */
import * as RequireProxy from "./RequireProxy";
import * as crash_handler from "../crash_handler";
import {Arguments, parseProcessArguments, processArguments} from "../shared/process-arguments";
import * as path from "path";
import * as electron from "electron";
import {remote} from "electron";

/*
import * as Sentry from "@sentry/electron";
Sentry.init({
    dsn: "https://72b9f40ce6894b179154e7558f1aeb87@o437344.ingest.sentry.io/5399791",
    appName: "TeaSpeak - Client",
    release: "TeaClient@" + electron.remote.app.getVersion()
});
*/

/* first of all setup crash handler */
parseProcessArguments();
if(!processArguments.has_flag(Arguments.NO_CRASH_RENDERER)) {
    const is_electron_run = process.argv[0].endsWith("electron") || process.argv[0].endsWith("electron.exe");
    crash_handler.initialize_handler("renderer", is_electron_run);
}

RequireProxy.initialize(path.join(__dirname, "backend-impl"), "main-app");

/* --------------- main initialize  --------------- */
import * as loader from "tc-loader";
import ipcRenderer = electron.ipcRenderer;

/* some decls */
declare global {
    interface Window {
        $: any;
        jQuery: any;
        jsrender: any;

        impl_display_critical_error: any;
        displayCriticalError: any;
        teaclient_initialize: any;
    }
}

loader.register_task(loader.Stage.INITIALIZING, {
    name: "teaclient initialize logging",
    function: async () => {
        (await import("./Logger")).setup();
    },
    priority: 80
});

loader.register_task(loader.Stage.INITIALIZING, {
    name: "teaclient initialize error",
    function: async () => {
        const _impl = message => {
            if(!processArguments.has_flag(Arguments.DEBUG)) {
                console.error("Displaying critical error: %o", message);
                message = message.replace(/<br>/i, "\n");

                const win = remote.getCurrentWindow();
                remote.dialog.showMessageBox({
                    type: "error",
                    buttons: ["exit"],
                    title: "A critical error happened!",
                    message: message
                });

                win.close();
            } else {
                console.error("Received critical error: %o", message);
                console.error("Ignoring error due to the debug mode");
            }
        };

        if(window.impl_display_critical_error) {
            window.impl_display_critical_error = _impl;
        } else {
            window.displayCriticalError = _impl;
        }
    },
    priority: 100
});

loader.register_task(loader.Stage.INITIALIZING, {
    name: "teaclient initialize arguments",
    function: async () => {
        if(processArguments.has_value(Arguments.DUMMY_CRASH_RENDERER))
            crash_handler.handler.crash();

        /* loader url setup */
        {
            const baseUrl = processArguments.value(Arguments.SERVER_URL);
            console.error(processArguments.value(Arguments.UPDATER_UI_LOAD_TYPE));
            if(typeof baseUrl === "string" && parseFloat((processArguments.value(Arguments.UPDATER_UI_LOAD_TYPE)?.toString() || "").trim()) === 3) {
                loader.config.baseUrl = baseUrl;
            }
        }
    },
    priority: 110
});

loader.register_task(loader.Stage.JAVASCRIPT, {
    name: 'gdb-waiter',
    function: async () => {
        if(processArguments.has_flag(Arguments.DEV_TOOLS_GDB)) {
            console.log("Process ID: %d", process.pid);
            await new Promise(resolve => {
                console.log("Waiting for continue!");

                const listener = () => {
                    console.log("Continue");
                    document.removeEventListener('click', listener);
                    resolve();
                };
                document.addEventListener('click', listener);
            });
            console.log("Awaited");
        }
    },
    priority: 10000
});

loader.register_task(loader.Stage.LOADED, {
    name: "argv connect",
    function: async () => {
        ipcRenderer.send('basic-action', "parse-connect-arguments");
    },
    priority: 0
});


loader.register_task(loader.Stage.JAVASCRIPT, {
    name: "teaclient load adapters",
    function: async () => {
        /* all files which replaces a native driver */
        try {
            await import("./hooks");
            await import("./SingleInstanceHandler");
            await import("./IconHelper");
            await import("./connection/FileTransfer");

            await import("./UnloadHandler");
            await import("./WindowsTrayHandler");
        } catch (error) {
            console.log(error);
            window.displayCriticalError("Failed to load native extensions: " + error);
            throw error;
        }
        remote.getCurrentWindow().on('focus', () => remote.getCurrentWindow().flashFrame(false));
    },
    /* Register all tasks after all javascript files have been loaded */
    priority: -1
});