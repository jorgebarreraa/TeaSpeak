import * as packager from "./package";
import * as deployer from "./deploy";
import * as glob from "glob";
import {parseVersion, Version} from "../modules/shared/version";

const fs = require("fs-extra");
const path = require("path");
const util = require('util');
const cproc = require('child_process');
const proc = require('process');
const ejs = require('ejs');
const exec = util.promisify(cproc.exec);
const ejs_render = util.promisify(ejs.renderFile);

const filename_update = "TeaClient-windows-x64.tar.gz";
const filename_installer = "TeaClient-windows-x64.exe";
const package_path = "build/TeaClient-win32-x64/";
const symbol_pdb_path = "native/build/symbols/";
const symbol_binary_path = package_path + "/resources/natives/";
let dest_path = undefined;
let info;
let version: Version;

function sign_command(file: string, description: string) {
    const base = "signtool sign /v /tr http://timestamp.digicert.com?alg=sha256 /td SHA256 /fd SHA256 /d \"" + description + "\" /du \"https://www.teaspeak.de/\"";
    return base + " /f " + path.join(__dirname, "../../../CodeSign-Certificate.pfx") + " /p qmsQBxpN2exj " + file;
}

async function sign_file(file: string, description: string) {
    const { stdout, stderr } = await exec(sign_command(file, description), {maxBuffer: 1024 * 1024 * 1024});
    console.log(stdout.toString());
}

async function make_template() : Promise<string> {
    const content = await ejs_render("installer/WinInstall.ejs", {
        source_dir: path.resolve(package_path) + "/*",
        dest_dir: path.resolve(dest_path),
        icon_file: path.resolve("resources/logo.ico"),
        version: info["version"],
        executable_name: filename_installer.substr(0, filename_installer.length - 4), //Remove the .exe
        sign_arguments:  sign_command("$f", "TeaClient installer")
    }, {});

    await fs.mkdirs(dest_path);
    fs.writeFileSync(dest_path + "/" + "installer.iss", content);
    return dest_path + "/" + "installer.iss";
}

async function make_installer(path: string) {
    console.log("Compiling path %s", path);
    const { stdout, stderr } = await exec("\"D:\\Program Files (x86)\\Inno Setup 6\\iscc.exe\" " + path, {maxBuffer: 1024 * 1024 * 1024}); //FIXME relative path?
}
if(process.argv.length < 3) {
    console.error("Missing build channel!");
    process.exit(1);
}

packager.pack_info(package_path).then(async info => {
    const asyncGlob = util.promisify(glob);
    const executables = await asyncGlob(package_path + "/**/*.exe");
    const ddls = await asyncGlob(package_path + "/**/*.dll");
    const nodeModules = await asyncGlob(package_path + "/**/*.node");

    const exe_description_padding = {
        "TeaClient.exe": "TeaClient v" + info["version"],
        "update-installer.exe": "TeaClient update installer"
    };
    const node_description_padding = {
        "teaclient_connection.node": "TeaClients connection library",
        "teaclient_crash_handler.node": "TeaClients crash handler",
        "teaclient_dns.node": "TeaClients dns resolve module",
        "teaclient_ppt.node": "TeaClients push to talk module"
    };

    console.log("Signing executables:");
    for(const executable of executables) {
        const desc = exe_description_padding[path.basename(executable)];
        if(!desc) throw "Missing description for " + executable;
        await sign_file(executable, "");
    }
    for(const dll of ddls)
        await sign_file(dll, "");
    for(const module of nodeModules) {
        const desc = node_description_padding[path.basename(module)];
        if(!desc) throw "Missing description for " + module;
        await sign_file(module, "");
    }

    return info;
}).then(async _info => {
    info = _info;
    version = parseVersion(_info["version"]);
    version.timestamp = Date.now();
    dest_path = "build/output/" + process.argv[2] + "/" + version.toString() + "/";
    await packager.pack_update(package_path, dest_path + "TeaClient-windows-x64.tar.gz");
}).then(async () => {
    await packager.write_info(dest_path + "info.json", "win32", "x64", filename_update, filename_installer)
}).then(async () => {
    await packager.write_version("build/output/version.json", "win32", "x64",  process.argv[2], version);
}).then(async () => await make_template())
.then(async path => await make_installer(path))
.then(async () => {
    console.log("Deploying PDB files");
    const files = [];
    for(const file of await fs.readdir(symbol_binary_path)) {
        if(!file.endsWith(".node")) {
            continue;
        }

        let file_name = path.basename(file);
        if(file_name.endsWith(".node")) {
            file_name = file_name.substr(0, file_name.length - 5);
        }

        const binary_path = path.join(symbol_binary_path, file);
        const pdb_path = path.join(symbol_pdb_path, file_name + ".pdb");
        if(!fs.existsSync(pdb_path)) {
            console.warn("Missing PDB file for binary %s", file);
            continue;
        }

        files.push({
            binary: binary_path,
            pdb: pdb_path
        });
    }
    await deployer.deploy_win_dbg_files(files, version);
    console.log("PDB files deployed");
}).then(async () => {
    await sign_file(dest_path + filename_installer, "TeaSpeak client installer");
}).then(async () => {
    console.log("Deploying build");
    await packager.deploy("win32", "x64", process.argv[2], version, dest_path + filename_update, dest_path + filename_installer, "exe");
}).then(() => {
    console.log("Succeed");
    proc.exit(0);
}).catch(error => {
    console.log(error);
    proc.exit(1);
});
