//__filename

module.paths.push("../../build/linux_x64");
module.paths.push("../../build/win32_64");

const electron = require("electron");
const crash_handler = require("teaclient_crash_handler");

if(process.argv.length != 3) {
    electron.app.on('ready', () => {
        console.log("SHow dialog");
        electron.dialog.showMessageBox({
            message: "Arguments: " + JSON.stringify(process.argv)
        });
        electron.app.exit();
    });
} else {
    crash_handler.setup_crash_handler(
        "test",
        __dirname + "/test_crash/",
        process.argv[0] + " " + __filename + " X app-success %crash_path%",
        process.argv[0] + " " + __filename + " X app-error %error_message%",
    );
    console.log(process.argv[0] + " " + __filename + " X app-success %crash_path%");
    console.log("Setup!");
    console.log("Crash!");
    crash_handler.crash();
}


export {};