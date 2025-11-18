import * as loader from "tc-loader";
import {Stage} from "tc-loader";
import {ClientServerSettingsStorage} from "../ClientStorage";
import {setServerSettingsStorage} from "tc-shared/ServerSettings";

loader.register_task(Stage.JAVASCRIPT_INITIALIZING, {
    name: "server storage init",
    function: async () => setServerSettingsStorage(new ClientServerSettingsStorage()),
    priority: 80
});