import * as electron from "electron";
import * as crash_handler from "./modules/crash_handler";
import * as child_process from "child_process";
import {app} from "electron";
//import * as Sentry from "@sentry/electron";

/*
Sentry.init({
    dsn: "https://72b9f40ce6894b179154e7558f1aeb87@o437344.ingest.sentry.io/5399791",
    appName: "TeaSpeak - Client",
    release: "TeaClient@" + electron.app.getVersion()
});
*/

/* just swallow this... */
process.on('uncaughtException', err => {
    console.error('Uncaught Exception thrown: %o', err);
    electron.app.whenReady().then(() => {
        electron.dialog.showMessageBox({
            type: "error",
            message: "An uncaught exception has reached the stack root.\nClosing application.",
            title: "Caught an uncaught exception"
        }).then(() => {
            process.exit(1);
        });
    });
});

/* We've to do this since we're removing all the bloat locales */
app.commandLine.appendSwitch('lang', 'en-US');

const is_electron_run = process.argv[0].endsWith("electron") || process.argv[0].endsWith("electron.exe");
const process_arguments = is_electron_run ? process.argv.slice(2) : process.argv.slice(1);
if(process_arguments.length > 0 && process_arguments[0] === "crash-handler") {
    /* crash handler callback */
    crash_handler.handle_crash_callback(process_arguments.slice(1));
} else if(process_arguments.length > 0 && process_arguments[0] === "dtest") {
    console.log("Executing installer");
    try {
        let pipe = child_process.spawn("\"C:\\Program Files (x86)\\TeaSpeak\\update-installer.exe\"", [], {
            detached: true,
            shell: true,
            cwd: "C:\\Program Files (x86)\\TeaSpeak",
            stdio: "ignore"
        });
    } catch(error) {
        console.dir(error);
    }

    setTimeout(() => app.exit(0), 2000);
} else {
    if(process_arguments.length > 0 && process_arguments[0] == "--main-crash-handler") {
        crash_handler.initialize_handler("main", is_electron_run);
    }

    /* app execute */
    {
        const versions = process.versions;
        console.log("Versions:");
        console.log("  TeaSpeak Client: " + electron.app.getVersion());

        for (const key of Object.keys(versions))
            console.log("  %s: %s", key, versions[key]);
    }

    const tea_client = require("./modules/core/main.js");
    tea_client.execute();
}