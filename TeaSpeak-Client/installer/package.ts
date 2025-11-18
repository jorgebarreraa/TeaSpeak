import * as fs from "fs-extra";
import * as tar from "tar-stream";
import * as zlib from "zlib";
import * as path from "path";
import * as asar from "asar";

import {Pack} from "tar-stream";
import * as request from "request";
import {Version} from "../modules/shared/version";

async function append_dir(parent: string, path: string, pack: Pack, excludes: (string | RegExp)[]) {
    const entries = await fs.readdir(parent + "/" + path);
    for(let entry of entries) {
        console.log(entry);
        const stat = await fs.stat(parent + "/" + path + "/" + entry);
        if(stat.isDirectory()) {
            console.log("Add sub: %s", entry);
            await append_dir(parent, path + "/" + entry, pack, excludes);
        } else {
            let exclude = false;
            for(const pattern of excludes) {
                if((path + "/" + entry).match(pattern)) {
                    console.log("Excluding file %s", path + "/" + entry);
                    exclude = true;
                    break;
                }
            }
            if(exclude) continue;

            let pentry = pack.entry({
                name: path + "/" + entry,
                size: stat.size,
                mode: stat.mode
            }, error => {
                if(error) throw error;
            });

            if(!pentry) throw "Failed to create new file";

            const pipe = fs.createReadStream(parent + "/" + path + "/" + entry).pipe(pentry);

            await new Promise((resolve, reject) => {
                pipe.on('finish', resolve);
                pipe.on('error', reject);
            });
            pentry.end();
        }
    }
}

export async function pack_update(source: string, dest: string) : Promise<string> {
    await fs.mkdirs(path.dirname(dest));
    const target = fs.createWriteStream(dest);
    const pack = tar.pack();
    const compress = zlib.createGzip();

    pack.pipe(compress).pipe(target);

    await append_dir(source, ".", pack, [/.\/app_versions\/.*/]); ///.\/postzip($|.exe)/,
    pack.finalize();

    await new Promise((resolve, reject) => {
        target.on('close', resolve);
        target.on('error', reject);
    });

    return dest;
}

export async function pack_info(src: string) : Promise<any> {
    const appAsarPath = path.join(src, 'resources/app.asar');
    const appPackageJSONPath = path.join(src, 'resources/app/package.json');

    if(await fs.pathExists(appAsarPath))
        return JSON.parse(asar.extractFile(appAsarPath, "package.json").toString());
    else
        return await fs.readJson(appPackageJSONPath);
}

interface InfoEntry {
    platform: string;
    arch: string;
    update: string;
    install: string;
}

export async function write_info(file: string, platform: string, arch: string, update_file: string, install_file: string) {
    let infos: InfoEntry[] = fs.existsSync(file) ? await fs.readJson(file) as InfoEntry[] : [];
    for(const entry of infos.slice()) {
        if(entry.platform == platform && entry.arch == arch)
            infos.splice(infos.indexOf(entry),1);
    }
    infos.push({
        "platform": platform,
        "arch": arch,
        "update": update_file,
        "install": install_file
    });
   await fs.writeJson(file, infos);
}

interface VersionFile {
    release: VersionEntry[];
    beta: VersionEntry[];
}

interface VersionEntry {
    platform: string;
    arch: string;
    version: Version;
}
export async function write_version(file: string, platform: string, arch: string, channel: string, version: Version) {
    let versions: VersionFile = fs.existsSync(file) ? await fs.readJson(file) as VersionFile : {} as any;

    versions[channel] = versions[channel] || [];
    let channel_data = versions[channel];

    for(const entry of channel_data.slice()) {
        if(entry.platform == platform && entry.arch == arch)
            channel_data.splice(channel_data.indexOf(entry), 1);
    }

    channel_data.push({
        platform: platform,
        arch: arch,
        version: version
    });
    await fs.writeJson(file, versions);
}
export async function deploy(platform: string, arch: string, channel: string, version: Version, update_file: string, install_file: string, install_suffix: string) {
    await new Promise(resolve => {
        const url = (process.env["teaclient_deploy_url"] || "https://clientapi.teaspeak.de/") + "api.php";
        console.log("Requesting " + url);
        console.log("Uploading update file " + update_file);
        console.log("Uploading install file " + install_file);
        console.log("Secret (env key: teaclient_deploy_secret): " + process.env["teaclient_deploy_secret"]);
        if(!process.env["teaclient_deploy_secret"]) throw "Missing secret!";


        request.post(url, {
            formData: {
                type: "deploy-build",
                secret: process.env["teaclient_deploy_secret"],

                platform: platform,
                arch: arch,
                version: JSON.stringify(version),
                channel: channel,

                update: fs.createReadStream(update_file),
                update_suffix: "tar.gz",

                installer: fs.createReadStream(install_file),
                installer_suffix: install_suffix
            }
        }, (error, response, body) => {
            if(error) {
                console.error("Failed to upload:");
                console.error(error);
                throw "Failed to upload: " + error;
            }
            console.log("Response code: " + (response ? response.statusCode : 0));
            let info;
            if(response && response.statusCode == 413) {
                info = {msg: "Files too large! Increase limits!"};
            } else {
                try {
                    info = JSON.parse(body);
                } catch (error) {
                    info = {};
                    console.dir(body);
                }
            }
            console.dir(info);
            if(!info["success"]) throw info["msg"] || "Could not deploy files!";
            resolve();
        });
    })
}