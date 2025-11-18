import {Options} from "electron-packager";
import * as packager from "electron-packager"
const pkg = require('../package.json');

if(pkg.name !== "TeaClient") {
    throw "The package name determines where the app data folder will be! Don't change that!"
}

import * as fs from "fs-extra";
import * as path_helper from "path";
import {parseVersion} from "../modules/shared/version";
import * as child_process from "child_process";
import * as os from "os";
import * as querystring from "querystring";
import request = require("request");
import * as deployer from "./deploy";
import AppInfoFile from "../modules/core/app-updater/AppInfoFile";

let options: Options = {} as any;
let version = parseVersion(pkg.version);
version.timestamp  = Date.now();

options.dir = '.';
options.name = "TeaClient";
options.appVersion = pkg.version;
options.appCopyright = "© 2018-2019 Markus Hadenfeldt All Rights Reserved";
options.out = "build/";

if(!pkg.dependencies['electron']) {
    console.error("Missing electron version");
    process.exit(1);
}

options["version-string"] = {
    'CompanyName': 'TeaSpeak',
    'LegalCopyright': options.appCopyright,
    'FileDescription' : 'TeaSpeak-Client',
    'OriginalFilename' : 'TeaClient.exe',
    'FileVersion' : pkg.version,
    'ProductVersion' : pkg.version,
    'ProductName' : 'TeaSpeak-Client',
    'InternalName' : 'TeaClient.exe'
};

options.electronVersion = pkg.dependencies['electron'];
options.protocols = [{name: "TeaSpeak - Connect", schemes: ["teaserver"]}];
options.overwrite = true;
options.derefSymlinks = true;
options.buildVersion = version.toString(true);
options.asar = true;

interface ProjectEntry {
    type: ProjectEntryType;
}

interface ProjectFile extends ProjectEntry {
    path: string;
    name: string | RegExp;

    //target_name?: string | ((file: string) => string);
}

interface ProjectDirectory extends ProjectEntry {

    path: string | RegExp;
    children?: boolean;
    files?: RegExp;
}

enum ProjectEntryType {
    FILE,
    DIRECTORY
}

const project_files: ProjectEntry[] = [];
{ /* general required files*/
    project_files.push({
        type: ProjectEntryType.FILE,
        path: "/",
        name: "package.json"
    } as ProjectFile);
    project_files.push({
        type: ProjectEntryType.FILE,
        path: "/",
        name: "main.js"
    } as ProjectFile);

    project_files.push({
        type: ProjectEntryType.DIRECTORY,
        path: "/node_modules",
        children: true,
        files: /\.(js|css|html|node|json)$/
    } as ProjectDirectory);

    project_files.push({
        type: ProjectEntryType.DIRECTORY,
        path: "/node_modules/electron",
        children: true,
        files: /.*/
    } as ProjectDirectory);
}

/* TeaClient modules */
project_files.push({
    type: ProjectEntryType.DIRECTORY,
    path: "/modules",
    children: true,
    files: /.*\.(js|css|html|png|svg)$/
} as ProjectDirectory);

/* resource files */
project_files.push({
    type: ProjectEntryType.DIRECTORY,
    path: "/resources"
} as ProjectDirectory);


if(process.argv.length < 4) {
    console.error("Missing process argument:");
    console.error("<win32/linux> <release/beta/nightly>");
    process.exit(1);
}

switch (process.argv[3]) {
    case "release":
    case "beta":
    case "nightly":
        break;

    default:
        console.error("Invalid release channel: %o", process.argv[3]);
        process.exit(1);
        break;

}

if (process.argv[2] == "linux") {
    options.arch = "x64";
    options.platform = "linux";
    options.icon = "resources/logo.svg";
} else if (process.argv[2] == "win32") {
    options.arch = "x64";
    options.platform = "win32";
    options.icon = "resources/logo.ico";
} else {
    console.error("Invalid system");
    process.exit(1);
}

const packagePathValidator = (path: string) => {
    path = path.replace(/\\/g,"/");

    const kIgnoreFile = true;
    const kAppendFile = false;

    const ppath = path_helper.parse(path);
    const is_directory = ppath.ext == '' && fs.statSync(path_helper.join('.', ppath.dir, ppath.name)).isDirectory();
    const directory = (is_directory ? path_helper.join(ppath.dir, ppath.name) : ppath.dir).replace(/\\/g,"/");

    //console.log("Is directory %o => %s", is_directory, directory);
    //console.dir(ppath);

    for(const entry of project_files) {
        if(entry.type == ProjectEntryType.DIRECTORY) {
            const dir_entry = <ProjectDirectory>entry;

            if(typeof(dir_entry.path) === 'string') {
                //ppath.dir == dir_entry.path
                //console.log("'" + dir_entry.path + "' | '" + directory + "'");
                if(dir_entry.path.startsWith(directory)) {
                    if(is_directory)
                        return kAppendFile;
                }
                if(directory == dir_entry.path) {
                    //console.log("Math: " + ppath.base + " to " + dir_entry.files);
                    if(dir_entry.files)
                        return ppath.base.match(dir_entry.files) ? kAppendFile : kIgnoreFile;
                    return kAppendFile;
                }
                if(directory.startsWith(dir_entry.path)) {
                    if(dir_entry.children) {
                        if(is_directory)
                            return kAppendFile;

                        const sub_path = directory.substr(dir_entry.path.length);
                        //console.log("Sub path: " + sub_path + ". Test: " + (path_helper.join(sub_path, ppath.base) + " against " + dir_entry.files);

                        if(dir_entry.files)
                            return path_helper.join(sub_path, ppath.base).match(dir_entry.files) ? kAppendFile : kIgnoreFile;
                        return kAppendFile;
                    }
                    //TODO test for regex
                }
            } else {
                //TODO
            }
        } else if(entry.type == ProjectEntryType.FILE) {
            const file_entry = <ProjectFile>entry;
            if(file_entry.path.startsWith(directory) && is_directory)
                return kAppendFile;

            if(directory == file_entry.path) {
                if(typeof(file_entry.name) === 'string' && ppath.base == file_entry.name)
                    return kAppendFile;
                else if (ppath.base.match(file_entry.name))
                    return kAppendFile;
            }
        }
    }

    return true;
};

options.ignore = path => {
    if(path.length == 0)
        return false; //Dont ignore root paths

    const ignore_path = packagePathValidator(path);
    if(!ignore_path) {
        console.log(" + " + path);
    } else {
        //console.log(" - " + path);
    }
    return ignore_path;
};

async function copy_striped(source: string, target: string, symbol_directory: string) {
    const exec = (command, options) => new Promise<{ stdout: Buffer | string, stderr: Buffer | string}>((resolve, reject) => child_process.exec(command, options, (error, out, err) => error ? reject(error) : resolve({stdout: out, stderr: err})));

    if(process.argv[2] == "win32") {
        await fs.copy(source, target);
        return;
    }
    if(process.argv[2] != "linux") throw "invalid target type";

    await fs.copy(source, target);

    {
        const symbols_command = await exec("dump_syms " + target, {
            maxBuffer: 1024 * 1024 * 512
        });
        if(symbols_command.stderr.length != 0) {
            console.error("Failed to create sys dump: %o", symbols_command.stderr.toString());
            throw "symbol create failed";
        }

        const symbols = symbols_command.stdout.toString();
        const header = symbols.substr(0, symbols.indexOf('\n'));
        const binary_name = path_helper.basename(target);
        const dump_id = header.split(" ")[3];
        if(binary_name != header.split(" ")[4])
            throw "binary name missmatch";

        const dump_dir = path_helper.join(symbol_directory, binary_name, dump_id);
        const dump_file = path_helper.join(dump_dir, binary_name + ".sym");
        if(!(await fs.pathExists(dump_dir)))
            await fs.mkdirp(dump_dir);
        await fs.ensureDir(dump_dir);

        console.log("Writing file to %s", dump_file);
        await fs.writeFile(dump_file, symbols);
        console.log("Created dump file for binary %s (%s).", binary_name, dump_id);
    }

    {
        console.log("Striping file");

        const strip_command = await exec("strip -s " + target, {
            maxBuffer: 1024 * 1024 * 512
        });
        if(strip_command.stderr.length != 0) {
            console.error("Failed to strip binary: %o", strip_command.stderr.toString());
            throw "strip failed";
        }

        const nm_command = await exec("nm " + target, {
            maxBuffer: 1024 * 1024 * 512
        });
        console.log("File stripped. Symbols left: \n%s", nm_command.stderr.toString().trim() || nm_command.stdout.toString().trim());
    }
}
async function create_native_addons(target_directory: string, symbol_directory: string) {
    let node_source = "native/build/" + os.platform() + "_" + os.arch() + "/";
    console.log("Native addon path: %s", node_source);
    await fs.ensureDir(target_directory);
    for(const file of await fs.readdir(node_source)) {
        if(!file.endsWith(".node")) {
            console.warn("Discovered non node file within node file out dir");
            continue;
        }

        await copy_striped(path_helper.join(node_source, file), path_helper.join(target_directory, file), symbol_directory);
    }
}

interface UIVersion {
    channel: string;
    version: string;
    git_hash: string;
    timestamp: number;

    required_client?: string;
    filename?: string;
}

async function downloadBundledUiPack(channel: string, targetDirectory: string) {
    const remote_url = "http://clientapi.teaspeak.dev/";

    const file = path_helper.join(targetDirectory, "bundled-ui.tar.gz");
    console.log("Creating default UI pack. Downloading from %s (channel: %s)", remote_url, channel);
    await fs.ensureDir(targetDirectory);

    let bundledUiInfo: UIVersion;
    await new Promise((resolve, reject) => {
        request.get(remote_url + "api.php?" + querystring.stringify({
            type: "ui-download",
            channel: channel,
            version: "latest"
        }), {
            timeout: 5000
        }).on('response', function(response) {
            if(response.statusCode != 200) {
                reject("Failed to download UI files (Status code " + response.statusCode + ")");
            }

            bundledUiInfo = {
                channel: channel,
                version: response.headers["x-ui-version"] as string,
                git_hash: response.headers["x-ui-git-ref"] as string,
                required_client: response.headers["x-ui-required_client"] as string,
                timestamp: parseInt(response.headers["x-ui-timestamp"] as string),
                filename: path_helper.basename(file)
            }
        }).on('error', error => {
            reject("Failed to download UI files: " + error);
        }).pipe(fs.createWriteStream(file)).on('finish', resolve);
    });

    if(!bundledUiInfo)
        throw "failed to generate ui info!";

    await fs.writeJson(path_helper.join(targetDirectory, "bundled-ui.json"), bundledUiInfo);
    console.log("UI-Pack downloaded!");
}

let path;
new Promise<string[] | string>((resolve, reject) => packager(options, (err, appPaths) => err ? reject(err) : resolve(appPaths))).then(async app_paths => {
    console.log("Copying changelog file!");
    /* We dont have promisify in our build system */
    await fs.copy(path_helper.join(options.dir, "github", "ChangeLog.txt"), path_helper.join(app_paths[0], "ChangeLog.txt"));
    return app_paths;
}).then(async app_paths => {
    await create_native_addons(path_helper.join(app_paths[0], "resources", "natives"), "build/symbols");
    return app_paths;
}).then(async app_paths => {
    await downloadBundledUiPack(process.argv[3], path_helper.join(app_paths[0], "resources", "ui"));
    return app_paths;
}).then(async appPaths => {
    path = appPaths[0];
    if(process.argv[2] == "linux") {
        await copy_striped(options.dir + "/native/build/exe/update-installer", path + "/update-installer", "build/symbols");
    } else if (process.argv[2] == "win32") {
        await copy_striped(options.dir + "/native/build/exe/update-installer.exe", path + "/update-installer.exe", "build/symbols");
    }

    await fs.writeJson(path + "/app-info.json", {
        version: 2,

        clientVersion: {
            timestamp: version.timestamp,
            buildIndex: version.build,
            patch: version.patch,
            minor: version.minor,
            major: version.major
        },

        clientChannel: process.argv[3],
        uiPackChannel: process.argv[3]
    } as AppInfoFile);

    return appPaths;
}).then(async app_path => {
    console.log("Fixing versions file");
    let version = await fs.readFile(path_helper.join(app_path[0], "version"), 'UTF-8');
    if(!version.startsWith("v"))
        version = "v" + version;
    await fs.writeFile(path_helper.join(app_path[0], "version"), version);
    return app_path;
}).then(async appPaths => {
    console.log("Removing locals folder");
    for(const locale of await fs.readdir(path_helper.join(appPaths[0], "locales"))) {
        if(locale.match(/en-US\.pak/)) {
            continue;
        }

        await fs.remove(path_helper.join(appPaths[0], "locales", locale))
    }
    return appPaths;
}).then(async () => {
    if(process.argv[2] == "win32") {
        console.log("Installing local PDB files");

        const symbol_binary_path = "native/build/" + os.platform() + "_" + os.arch() + "/";
        const symbol_pdb_path =  "native/build/symbols/";
        const symbol_server_path = path_helper.join(__dirname, "..", "native", "build", "symbol-server");

        const files = [];
        for(const file of await fs.readdir(symbol_binary_path)) {
            console.error(file);
            if(!file.endsWith(".node"))
                continue;
            let file_name = path_helper.basename(file);
            if(file_name.endsWith(".node"))
                file_name = file_name.substr(0, file_name.length - 5);
            const binary_path = path_helper.join(symbol_binary_path, file);
            const pdb_path = path_helper.join(symbol_pdb_path, file_name + ".pdb");
            if(!fs.existsSync(pdb_path)) {
                console.warn("Missing PDB file for binary %s", file);
                continue;
            }
            files.push({
                binary: binary_path,
                pdb: pdb_path
            });
        }

        console.log("Gathered %d files", files.length);
        await deployer.deploy_win_dbg_files(files, version, symbol_server_path);
        console.log("PDB files deployed");
    }
}).then(() => {
    console.log("Package created");
    process.exit(0);
}).catch(error => {
    console.error(error);
    console.error("Failed to create package!");
    process.exit(1);
});

export {}