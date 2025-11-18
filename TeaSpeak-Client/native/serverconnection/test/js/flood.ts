import "./RequireHandler";

import * as handle from "tc-native/connection";
import {NativeServerConnection} from "tc-native/connection";

//remote_host: "51.68.181.92",
//remote_host: "94.130.236.135",
//remote_host: "54.36.232.11", /* the beast */
//remote_host: "79.133.54.207", /* gommehd.net */

const target_address = "127.0.0.1";
const { host, port } = {
    host: target_address.split(":")[0],
    port: target_address.split(":").length > 1 ? parseInt(target_address.split(":")[1]) : 9987
};

function executeDownload(transferKey: string) {
    let target = new ArrayBuffer(1024);
    const source = handle.ft.download_transfer_object_from_buffer(target);

    const transfer = handle.ft.spawn_connection({
        client_transfer_id: 0,
        server_transfer_id: 0,

        object: source,
        transfer_key: transferKey,

        remote_address: "localhost",
        remote_port: 30333
    });

    transfer.callback_failed = message => {
        console.log("[FT] failed: %o", message);
    };

    transfer.callback_finished = aborted => {
        console.log("[FT] done (Aborted %o)", aborted);
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
}

class Bot {
    connection: NativeServerConnection;
    knwonChannelIds: number[] = [];
    client_id: number;
    initialized: boolean;

    private switchInterval = [];
    private _timeouts = [];

    reset() {
        this.connection?.disconnect("reset", () => {});
        this.connection = undefined;
        for(const interval of this.switchInterval)
            clearInterval(interval);
        for(const timeouts of this._timeouts)
            clearInterval(timeouts);
    }

    connect(callbackConnected?: () => void) {
        this.knwonChannelIds = [];
        this.client_id = 0;
        this.initialized = false;

        this.connection = handle.spawn_server_connection();
        this.connection.connect({
            timeout: 5000,
            remote_port: port,
            remote_host: host,
            callback: error => {
                if(callbackConnected) {
                    callbackConnected();
                }

                if(error == 0) {
                    this.connection.send_command("clientinit", [
                        {
                            "client_key_offset": 2030434,
                            /*
                            "client_version": "1.0.0",
                            "client_platform": "nodejs/linux",
                            */
                            "client_version": "3.1.8 [Build: 1516614607]",
                            "client_platform": "Windows",
                            "client_version_sign": "gDEgQf/BiOQZdAheKccM1XWcMUj2OUQqt75oFuvF2c0MQMXyv88cZQdUuckKbcBRp7RpmLInto4PIgd7mPO7BQ==",

                            "client_nickname": "TeaClient Native Module Test",

                            "client_input_hardware":true,
                            "client_output_hardware":true,
                            "client_default_channel":"",
                            "client_default_channel_password":"",
                            "client_server_password":"",
                            "client_meta_data":"",
                            "client_nickname_phonetic":"",
                            "client_default_token":"",
                            "hwid":"123,456123123123",
                            return_code:91
                        }
                    ], []);
                } else {
                    console.log("Bot connect failed: %o (%s) ", error, this.connection.error_message(error));
                    this.reset();
                }
            },

            identity_key: "MG4DAgeAAgEgAiBC9JsqB1am6vowj2obomMyxm1GLk8qyRoxpBkAdiVYxwIgWksaSk7eyVQovZwPZBuiYHARz/xQD5zBUBK6e63V7hICIQCZ2glHe3kV62iIRKpkV2lzZGZtfBPRMbwIcU9aE1EVsg==",
            teamspeak: true
        });

        this.connection.callback_command = (command, args, switches) => this.handle_command(command, args);
        this.connection.callback_disconnect = () => {
            this.connection = undefined;
            this.reset();
        }
    }

    async disconnect() {
        await new Promise(resolve => this.connection.disconnect("bb", resolve));
        this.connection = undefined;
        this.reset();
    }

    private handle_command(command: string, args: any[]) {
        if(command == "initserver") {
            this.client_id = parseInt(args[0]["aclid"]);
        } else if(command == "channellistfinished"){
             this.initialized = true;
             //this.switchInterval.push(setInterval(() => this.switch_channel(), 10_000 + Math.random() * 5_000));
            setTimeout(() => this.disconnect(), 2500);
        } else if(command == "channellist") {
            for(const element of args) {
                this.knwonChannelIds.push(parseInt(element["cid"]));
                if((parseInt(element["channel_icon_id"]) >>> 0) > 1000) {
                    this.connection.send_command("ftinitdownload", [{
                        "clientftfid": "1",
                        "seekpos": "0",
                        "name": "/icon_" + element["channel_icon_id"],
                    }], []);
                }
            }
        } else if(command == "notifychannelcreated") {
            this.knwonChannelIds.push(parseInt(args[0]["cid"]));
        } else if(command == "notifychanneldeleted") {
            for(const arg of args) {
                const channel_id = parseInt(arg["cid"]);
                const index = this.knwonChannelIds.indexOf(channel_id);
                if(index >= 0) {
                    this.knwonChannelIds.splice(index, 1);
                }
            }
        } else if(command === "notifystartdownload") {
            console.log("File transfer created: %o", args[0]["ftkey"]);
            executeDownload(args[0]["ftkey"]);
        }
    }

    private switch_channel() {
        const target_channel = this.knwonChannelIds[Math.floor((Math.random() * 100000) % this.knwonChannelIds.length)];
        console.log("Switching to channel %d", target_channel);
        this.connection.send_command("clientmove", [{clid: this.client_id, cid: target_channel}], []);
    }
}


const botList: Bot[] = [];

async function connectBots() {
    for(let index = 0; index < 200; index++) {
        console.error("CONNECT");
        const bot = new Bot();
        botList.push(bot);
        await new Promise(resolve => bot.connect(resolve));

        //await new Promise(resolve => setTimeout(resolve, 250));
        while(botList.length > 50) {
            const [ bot ] = botList.splice(0, 1);
            bot.reset();
        }
    }
}

setInterval(() => {
    botList.forEach(connection => {
        if(connection.connection) {
            //connection.connection.send_voice_data(new Uint8Array([1, 2, 3]), 5, false);
        } else {
            connection.connect();
        }
    });
}, 250);

connectBots().then(undefined);