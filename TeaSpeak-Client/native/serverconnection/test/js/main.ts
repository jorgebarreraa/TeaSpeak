import "./RequireHandler";

const kPreloadAsan = false;
if(kPreloadAsan) {
    //LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libasan.so.5
    const os = require('os');
    // @ts-ignore
    process.dlopen(module, '/usr/lib/x86_64-linux-gnu/libasan.so.5', os.constants.dlopen.RTLD_NOW);
}

import * as handle from "tc-native/connection";

const connection_list = [];
const connection = handle.spawn_server_connection();
const client_list = [];

//process.exit(0);
connection.callback_command = (command, args, switches) => {
    //console.log("Got command %s: %o (%o)", command, args, switches);
    if(command === "notifycliententerview") {
        for(const client_args of args) {
            console.log(client_args["clid"]);
            const client = connection._voice_connection.register_client(parseInt(client_args["clid"]));
            console.log("Registered voice client %o", client);
            client_list.push(client);

            console.log("Applying volume of 0.2");
            client.set_volume(0.2);

            const stream = client.get_stream();
            console.log("Client buffer latency: %d/%d: %o", stream.get_buffer_latency(), stream.get_buffer_max_latency(), stream);
            stream.set_buffer_latency(0.02);
            stream.set_buffer_max_latency(0.2);

            client.callback_playback = () => {
                console.log("Client %d started speaking", client.client_id);
            };

            client.callback_stopped = () => {
                console.log("Client %d stopped speaking", client.client_id);
            };

            client.callback_state_changed = state => {
                console.log("Client %d change state to %d (%s)", client.client_id, state, handle.PlayerState[state]);
            };

        }
    }
};

connection.callback_disconnect = reason => {
    console.log("Got disconnect: %s", reason);
};

const do_connect = (connection) => {
    connection.connect({
        timeout: 500_000,
        remote_port: 9987,
        //remote_host: "188.40.240.20", /* twerion */
        remote_host: "127.0.0.1",
        //remote_host: "ts.teaspeak.de",
        //remote_host: "51.68.181.92",
        //remote_host: "94.130.236.135",
        //remote_host: "54.36.232.11", /* the beast */
        //remote_host: "79.133.54.207", /* gommehd.net */
        callback: error => {
            console.log("Connected with state: %o (%s) ", error, connection.error_message(error));

            if(error == 0) {
                connection.send_command("handshakebegin", [{intention: 0, authentication_method: 2, client_nickname: "Native client test" }], []);
                console.dir(handle.ServerType);
                console.log("Server type: %o|%o", connection.server_type, handle.ServerType[connection.server_type]);
                connection.send_command("clientinit", [
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
                        return_code: 91
                    }
                ], []);
            }
        },

        identity_key: "MG4DAgeAAgEgAiBC9JsqB1am6vowj2obomMyxm1GLk8qyRoxpBkAdiVYxwIgWksaSk7eyVQovZwPZBuiYHARz/xQD5zBUBK6e63V7hICIQCZ2glHe3kV62iIRKpkV2lzZGZtfBPRMbwIcU9aE1EVsg==",
        teamspeak: false /* used here to speed up the handshake process :) */
    });

    connection.callback_voice_data = (buffer, client_id, codec_id, flag_head, packet_id) => {
        console.log("Having data!");
        connection.send_voice_data(buffer, codec_id, flag_head);
    };

    connection.callback_disconnect = error => {
        console.log("Disconnect: %s", error);
    };

    connection.callback_command = (command, arguments1, switches) => {
        if(command === "notifyservergrouppermlist") {
            console.log("Received perm list");
            return;
        } else if(command === "notifyservergroupclientlist") {
            console.log("Perm group");
            return;
        } else if(command === "notifypermissionlist") {
            console.log("Received permission list");
            return;
        } else if(command == "notifycliententerview") {
            console.log("Enter client: %o", arguments1);
            return;
        } else if(command === "error") {
            console.log("Received error: %o", arguments1);
            return;
        }

        console.log("Command %s: %o", command, arguments1);
    };

    //connection._voice_connection.register_client(2);
};
do_connect(connection);

connection.callback_voice_data = (buffer, client_id, codec_id, flag_head, packet_id) => {
    console.log("Received voice of length %d from client %d in codec %d (Head: %o | ID: %d)", buffer.byteLength, client_id, codec_id, flag_head, packet_id);
    connection.send_voice_data(buffer, codec_id, flag_head);
};

setInterval(() => {
    if("gc" in global) {
        console.log("GC");
        global.gc();
    }
}, 1000);

/* keep the object alive */
setTimeout(() => {
    connection.connected();
}, 1000);

connection_list.push(connection);
export default {};