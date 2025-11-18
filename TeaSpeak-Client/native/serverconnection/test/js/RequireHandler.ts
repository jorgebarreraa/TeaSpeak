import * as path from "path";
import * as os from "os";

const Module = require("module");

const originalRequire = Module._load;
Module._load = (module, ...args) => {
    if(module === "tc-native/connection") {
        let build_type;
        console.error(os.platform());
        if(os.platform() === "win32") {
            build_type = "win32_x64";
        } else {
            build_type = "linux_x64";
        }

        return originalRequire(path.join(__dirname, "..", "..", "..", "build", build_type, "teaclient_connection.node"), ...args);
    } else {
        return originalRequire(module, ...args);
    }
};

export = {};