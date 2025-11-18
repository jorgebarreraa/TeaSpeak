const installer = require("electron-installer-debian");
import * as packager from "./package";
import {parseVersion, Version} from "../modules/shared/version";

const package_path = "build/TeaClient-linux-x64/";
const filename_update = "TeaClient-linux-x64.tar.gz";

let options = {
    src: package_path,
    dest: undefined,
    dest_file: undefined,
    arch: 'amd64',

    rename: (directory, name) => {
        console.log("Destination directory: " + directory);
        console.log("Destination name     : " + name);
        options.dest_file = directory + "/" + name;
        return directory + "/" + name;
    },
    options: {
        name: "TeaClient",
        productName: "TeaClient",
        genericName: "TeaSpeak - Client",
        description: "TeaClient by TeaSpeak",
        version: undefined,
        homepage: "https://teaspeak.de",
        maintainer: "WolverinDEV <client.support@teaspeak.de>",

        icon: 'resources/logo.svg',
        categories: [
            "Utility"
        ],
        bin: 'TeaClient',

        recommends: [],

    }
};

if(process.argv.length < 3) {
    console.error("Missing build channel!");
    process.exit(1);
}

let version: Version;
const alive = setInterval(() => {}, 1000);
packager.pack_info(package_path).then(package_info => {
    options.options.version = (version = parseVersion(package_info["version"])).toString();
    options.dest = "build/output/" + process.argv[2] + "/" + options.options.version + "/";
    console.log('Creating package for version ' + options.options.version + ' (this may take a while)');

    //return Promise.resolve();
    return installer(options);
}).then(() => {
    if(!options.dest_file)
        options.dest_file = options.dest + "TeaClient_" + options.options.version + "_amd64.deb";

    console.log(`Successfully created package at ${options.dest} (${options.dest_file})`);
    return packager.pack_update(options.src, options.dest + "/" + filename_update);
}).then(() => {
    return packager.write_info(options.dest + "info.json", "linux", "x64", filename_update, options.dest_file)
}).then(() => {
    return packager.write_version("build/output/version.json", "linux", "x64",  process.argv[2], version);
}).then(() => {
    console.log("Deploying symbol files");
    //FIXME!
}).then(() => {
    //Fixup in case of skip of the packaging

    console.log("Deploying build");
    return packager.deploy("linux", "x64", process.argv[2], version, options.dest + filename_update, options.dest_file, "deb");
}).then(() => {
    console.log("Build version (" + options.options.version + ") created!");
    clearInterval(alive);
})
.catch(err => {
    console.error("Failed to pack package!");
    console.error(err, err.stack);
    process.exit(1)
});

