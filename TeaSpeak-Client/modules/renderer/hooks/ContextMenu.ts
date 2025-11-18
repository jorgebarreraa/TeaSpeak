import * as loader from "tc-loader";
import {setGlobalContextMenuFactory} from "tc-shared/ui/ContextMenu";
import {ClientContextMenuFactory} from "../ContextMenu";

loader.register_task(loader.Stage.JAVASCRIPT_INITIALIZING, {
    name: "context menu",
    function: async () => {
        setGlobalContextMenuFactory(new ClientContextMenuFactory());
    },
    priority: 60
});