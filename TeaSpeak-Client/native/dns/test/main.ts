/// <reference path="../exports/exports.d.ts" />

console.log("HELLO WORLD");
module.paths.push("../../build/linux_x64");
module.paths.push("../../build/win32_64");

const original_require = require;
require = (module => original_require("/home/wolverindev/TeaSpeak-Client/client/native/build/linux_x64/" + module + ".node")) as any;
import * as handle from "teaclient_dns";
require = original_require;

handle.initialize();
console.log("INITS");
handle.resolve_cr("voice.teamspeak.com", 9987, result => {
    console.log("Result: " + JSON.stringify(result));
});
handle.resolve_cr("ts.twerion.net", 9987, result => {
    console.log("Result: " + JSON.stringify(result));
});
handle.resolve_cr("twerion.net", 9987, result => {
    console.log("Result: " + JSON.stringify(result));
});
/*
handle.resolve_cr("localhost", 9987, result => {
    console.log("Result: " + JSON.stringify(result));
});
*/