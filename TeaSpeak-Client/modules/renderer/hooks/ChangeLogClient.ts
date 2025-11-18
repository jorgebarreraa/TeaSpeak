import * as loader from "tc-loader";
import {Stage} from "tc-loader";
import {ClientUpdater} from "../ClientUpdater";
import {setNativeUpdater} from "tc-shared/update";

loader.register_task(Stage.JAVASCRIPT_INITIALIZING, {
    name: "web updater init",
    function: async () => {
        const updater = new ClientUpdater();
        await updater.initialize();
        setNativeUpdater(updater);
    },
    priority: 50
});