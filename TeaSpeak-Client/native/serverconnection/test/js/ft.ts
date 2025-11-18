/// <reference path="../../exports/exports.d.ts" />
module.paths.push("../../build/linux_amd64");
module.paths.push("../../build/win32_64");

import * as fs from "fs";
import * as net from "net";
import * as os from "os";
import * as path from "path";

const original_require = require;
require = (module => original_require(path.join(__dirname, "..", "..", "..", "build", os.platform() + "_" + os.arch(), module + ".node"))) as any;
import * as handle from "teaclient_connection";
require = original_require;

const buffer_size = 24;
const start_server: () => Promise<string> = async () => {
    const server: net.Server = net.createServer();
    await new Promise(resolve => server.listen(30303, "localhost", resolve));

    server.on('connection', socket => {
        console.log("[SERVER] Received new client from %s:%d", socket.remoteAddress, socket.remotePort);

        let key: string = "";
        socket.on('data', buffer => {
            if(key.length < 16) {
                key += buffer.toString();
                if(key.length >= 16) {
                    console.log("Received key: %s", key.substr(0, 16));
                    if(key.length > 16) {
                        console.log("Overhead: %s", key.substr(16));
                        key = key.substr(0, 16);
                    }

                    if(key == "ft_upload_data__") {

                    } else {
                        //They expect stuff
                        socket.write("123456789123456789aabb222"); //Must be equal to buffer_size
                    }
                }
            }
            //console.log("[SERVER] Received data: %s", buffer.toString());
        });
    });

    const address = server.address();
    console.log("[SERVER] Listening on %o", address);
    return typeof address === "string" ? address : address.address;
};

function str2ab(str) {
    var buf = new ArrayBuffer(str.length); // 2 bytes for each char
    var bufView = new Uint8Array(buf);
    for (var i=0, strLen=str.length; i<strLen; i++) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}

start_server().catch(error => {
    console.error("Failed to start FT server (%o)", error);
}).then(address => {
    const target_buffer = new Uint8Array(buffer_size);
    const destination = handle.ft.download_transfer_object_from_buffer(target_buffer.buffer);
    //const source = handle.ft.upload_transfer_object_from_buffer(str2ab("Hello World"));
    //console.log(source);
    //const source = handle.ft.upload_transfer_object_from_file(__dirname, "test_upload.txt");
    //const source = handle.ft.upload_transfer_object_from_file("/home/wolverindev/Downloads", "xxx.iso");
    const source = handle.ft.upload_transfer_object_from_file("C:\\Users\\WolverinDEV\\Downloads", "ütest.txt");

    console.log(source);
    const upload = true;

    const transfer = handle.ft.spawn_connection({
        client_transfer_id: 0,
        server_transfer_id: 0,

        object: upload ? source : destination,
        transfer_key: upload ? "ft_upload_data__" : "ft_download_data",

        remote_address: address as any,
        remote_port: 30303
    });

    transfer.callback_failed = message => {
        console.log("[FT] failed: %o", message);
    };

    transfer.callback_finished = aborted => {
        console.log("[FT] done (Aborted %o)", aborted);
        if(!upload)
            console.log("[FT] Buffer: %o", String.fromCharCode.apply(null, target_buffer));
        //console.log("A: %o", transfer);
    };

    let last = 0;
    transfer.callback_progress = (current, max) => {
        const diff = current - last;
        last = current;
        console.log("[FT] Progress: %d|%d (%d) %dmb/s", current, max, Math.ceil(current / max * 100), Math.ceil(diff / 1024 / 1024));
    };

    transfer.callback_start = () => {
        console.log("[FT] start");
    };

    transfer.start();
});

setInterval(() => {
    if(global && global.gc)
        global.gc();
}, 1000);