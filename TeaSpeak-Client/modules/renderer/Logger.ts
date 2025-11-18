enum LogType {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR
}

export interface Logger {
    trace(message: string, ...args);
    debug(message: string, ...args);
    info(message: string, ...args);
    log(message: string, ...args);
    warning(message: string, ...args);
    error(message: string, ...args);

    dir_error(error: any, message?: string);
}

let loggers = {};
const original_console: Console = {} as any;

export function setup() {
    Object.assign(original_console, console);
    Object.assign(console, logger("console"));
}

export function logger(name: string = "console") : Logger {
    if(loggers[name])
        return loggers[name];

    return loggers[name] = create_logger(name);
}

import * as util from "util";
function create_logger(name: string) : Logger {
    const log = (type, message: string, ...args) => {
        switch (type) {
            case LogType.TRACE:
                original_console.debug(message, ...args);
                break;
            case LogType.DEBUG:
                original_console.debug(message, ...args);
                break;
            case LogType.INFO:
                original_console.info(message, ...args);
                break;
            case LogType.WARNING:
                original_console.warn(message, ...args);
                break;
            case LogType.ERROR:
                original_console.error(message, ...args);
                break;
        }

        const log_message = util.format(message, ...args);
        process.stdout.write(util.format("[%s][%s] %s", name, LogType[type], log_message) + "\n");
    };

    return {
        trace: (m, ...a) => log(LogType.TRACE, m, ...a),
        debug: (m, ...a) => log(LogType.DEBUG, m, ...a),
        info: (m, ...a) => log(LogType.INFO, m, ...a),
        log: (m, ...a) => log(LogType.INFO, m, ...a),
        warning: (m, ...a) => log(LogType.WARNING, m, ...a),
        error: (m, ...a) => log(LogType.ERROR, m, ...a),

        dir_error: (e, m) => {
            log(LogType.ERROR, "Caught exception: " + m);
            log(LogType.ERROR, e);
        }
    };
}

(window as any).logger = {
    log: function (category, level, message) {
        console.log("%d | %d | %s", category, level, message);
    }
};