import * as path from "path";
import {remote} from "electron";
import * as electron from "electron";
import * as os from "os";

const Module = require("module");

interface ModuleOverride {
    name?: string,
    test: string | RegExp | ((request: string) => boolean);
    callback: (this: string, request: string, parent?: NodeJS.Module) => any;
}
const overrides: ModuleOverride[] = [];

function proxied_load(request: string, parent?: NodeJS.Module) {
    for(const override of overrides) {
        let test_satisfied = false;
        if(typeof override.test === "string") {
            test_satisfied = override.test === request;
        } else if(typeof override.test === "function") {
            test_satisfied = override.test(request);
        } else if(typeof override === "object") {
            if(override.test instanceof RegExp) {
                test_satisfied = !!request.match(override.test);
            }
        }

        if(test_satisfied) {
            //console.log("Using override %s for %s", override.name || "unnamed", request);
            return override.callback.apply(this, arguments);
        }
    }
    //console.log("No override found for %s", request);
    return proxied_load.original_load.apply(this, arguments);
}

function shared_backend_loader(request: string) {
    if(!request.startsWith("tc-backend/")) {
        throw "invalid target";
    }

    if(!backend_root) {
        throw "backend is not available in this context";
    }

    const target = request.substr(11);
    return require(path.join(backend_root, target));
}

namespace proxied_load {
    export let original_load: typeof Module.require;
}

let backend_root: string;
let app_module: string;
export function initialize(backend_root_: string, app_module_: string) {
    backend_root = backend_root_;
    app_module = app_module_;

    proxied_load.original_load = Module._load;
    Module._load = proxied_load;

    window["backend-loader"] = {
        require: shared_backend_loader
    };
}


overrides.push({
    name: "tc-loader",
    test: "tc-loader",
    callback: () => window["loader"]
});

overrides.push({
    name: "native loader",
    test: /^tc-native\/[a-zA-Z_-]+$/,
    callback: request => {
        const name = request.substr(10);

        const file_mapping = {
            connection: "teaclient_connection.node",
            ppt: "teaclient_ppt.node",
            dns: "teaclient_dns.node"
        };

        if(typeof file_mapping[name] !== "string")
            throw "unknown native module";

        const app_path = (remote || electron).app.getAppPath();
        let target_path;
        if(app_path.endsWith(".asar")) {
            target_path = path.join(path.dirname(app_path), "natives", file_mapping[name]);
        } else {
            /* from source code */
            target_path = path.join(app_path, "native", "build", os.platform() + "_" + os.arch(), file_mapping[name]);
        }
        return require(target_path);
    }
});

function resolveModuleMapping(context: string, resource: string) {
    if(context.endsWith("/")) {
        context = context.substring(0, context.length - 1);
    }

    const loader = require("tc-loader");

    const mapping = loader.module_mapping().find(e => e.application === app_module); //FIXME: Variable name!
    if(!mapping) {
        debugger;
        throw "missing ui module mapping";
    }

    const entries = mapping.modules.filter(e => e.context === context);
    if(!entries.length) {
        debugger;
        throw "unknown target path";
    }

    const entry = entries.find(e => path.basename(e.resource, path.extname(e.resource)) === resource);
    if(!entry) {
        if(resource.indexOf(".") === -1 && !resource.endsWith("/"))
            return resolveModuleMapping(context + "/" + resource, "index");

        throw "unknown import (" + context + "/" + resource + ")";
    }

    return entry.id;
}

overrides.push({
    name: "svg sprites",
    test: /^svg-sprites\/.*/,
    callback: request => {
        const entryId = resolveModuleMapping("svg-sprites", request.substring("svg-sprites/".length));
        return window["shared-require"](entryId);
    }
});

overrides.push({
    name: "shared loader",
    test: /^tc-shared\/.*/,
    callback: request => {
        if(request.endsWith("/")) {
            return require(request + "index");
        }

        let contextPath = path.dirname(request.substr(10));
        const entryId = resolveModuleMapping("shared/js/" + (contextPath === "." ? "" : contextPath), path.basename(request, path.extname(request)));
        return window["shared-require"](entryId);
    }
});

overrides.push({
    name: "tc-events",
    test: "tc-events",
    callback: () => {
        const entryId = resolveModuleMapping("vendor/TeaEventBus/src/", "index");
        return window["shared-require"](entryId);
    }
});

overrides.push({
    name: "tc-services",
    test: "tc-services",
    callback: () => {
        const entryId = resolveModuleMapping("vendor/TeaClientServices/src/", "index");
        return window["shared-require"](entryId);
    }
});
