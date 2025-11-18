import {setStorageAdapter} from "tc-shared/StorageAdapter";
import {clientStorage, initializeClientStorage} from "../ClientStorage";
import * as loader from "tc-loader";
import {Stage} from "tc-loader";

loader.register_task(Stage.JAVASCRIPT_INITIALIZING, {
    name: "storage init",
    function: async () => {
        await initializeClientStorage();
        setStorageAdapter(clientStorage);
    },

    /* Must come before everything else! */
    priority: 10_000
});