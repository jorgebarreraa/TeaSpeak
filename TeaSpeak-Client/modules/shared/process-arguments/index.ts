import * as electron from "electron";
import {app} from "electron";

export class Arguments {
    static readonly DEV_TOOLS = ["t", "dev-tools"];
    static readonly DEV_TOOLS_GDB = ["gdb"];
    static readonly DEBUG = ["d", "debug"];
    static readonly DISABLE_ANIMATION = ["a", "disable-animation"];
    static readonly SERVER_URL = ["u", "server-url"];
    static readonly UPDATER_UI_DEBUG = ["updater-debug-ui"];
    static readonly UPDATER_ENFORCE = ["updater-enforce"];
    static readonly UPDATER_CHANNEL = ["updater-channel"];
    static readonly UPDATER_LOCAL_VERSION = ["updater-local-version"];
    static readonly UPDATER_UI_LOAD_TYPE = ["updater-ui-loader_type"];
    static readonly UPDATER_UI_NO_CACHE = ["updater-ui-no-cache"];
    static readonly UPDATER_UI_IGNORE_VERSION = ["updater-ui-ignore-version"];
    static readonly DISABLE_HARDWARE_ACCELERATION = ["disable-hardware-acceleration"];
    static readonly NO_SINGLE_INSTANCE = ["no-single-instance"];
    static readonly DUMMY_CRASH_MAIN = ["dummy-crash-main"];
    static readonly DUMMY_CRASH_RENDERER = ["dummy-crash-renderer"];
    static readonly NO_CRASH_RENDERER = ["no-crash-renderer"];

    has_flag: (...keys: (string | string[])[]) => boolean;
    has_value: (...keys: (string | string[])[]) => boolean;
    value: (...keys: (string | string[])[]) => string;
}

export interface Window {
    process_args: Arguments;
}

export const processArguments: Arguments = {} as Arguments;

export function parseProcessArguments() {
    if(!process || !process.type || process.type === 'renderer') {
        Object.assign(processArguments, electron.remote.getGlobal("process_arguments"));
        (window as any).process_args = processArguments;
    } else {
        const is_electron_run = process.argv[0].endsWith("electron") || process.argv[0].endsWith("electron.exe");

        {
            const minimist: <T> (args, opts) => T = require("./minimist") as any;
            let args = minimist<Arguments>(is_electron_run ? process.argv.slice(2) : process.argv.slice(1), {
                boolean: true,
                "--": true
            }) as Arguments;
            args.has_flag = (...keys) => {
                for(const key of [].concat(...Array.of(...keys).map(e => Array.isArray(e) ? Array.of(...e) : [e])))
                    if(typeof processArguments[key as any as string] === "boolean")
                        return processArguments[key as any as string];
                return false;
            };

            args.value = (...keys) => {
                for(const key of [].concat(...Array.of(...keys).map(e => Array.isArray(e) ? Array.of(...e) : [e])))
                    if(typeof processArguments[key] !== "undefined")
                        return processArguments[key];
                return undefined;
            };

            args.has_value = (...keys) => {
                return args.value(...keys) !== undefined;
            };

            if(args.has_flag(Arguments.DEBUG)) {
                const _has_flag = args.has_flag;
                args.has_flag = (...keys) => {
                    const result = _has_flag(...keys);
                    console.log("Process argument test for parameter %o results in %o", keys, result);
                    return result;
                };

                const _value = args.value;
                args.value = (...keys) => {
                    const result = _value(...keys);
                    console.log("Process argument test for parameter %o results in %o", keys, result);
                    return result;
                }
            }
            console.log("Parsed CMD arguments %o as %o", process.argv, args);
            Object.assign(processArguments, args);
            Object.assign(global["process_arguments"] = {}, args);
        }

        if(processArguments.has_flag("help", "h")) {
            console.log("TeaClient command line help page");
            console.log(" -h --help => Displays this page");
            console.log(" -d --debug => Enabled the application debug");
            console.log(" -t --dev-tools => Enables dev tools");
            console.log(" -u --server-url => Sets the remote client api server url");
            console.log("    --updater-channel => Set the updater channel");
            console.log(" -a --disable-animation => Disables some cosmetic animations and loadings");
            console.log("    --disable-hardware-acceleration => Disables the hardware acceleration for the UI");
            console.log("    --no-single-instance => Disable multi instance testing");

            //is_debug = process_args.has_flag("debug", "d");
            //open_dev_tools = process_args.has_flag("dev-tools", "dt");
            app.exit(0);
        }
    }
}