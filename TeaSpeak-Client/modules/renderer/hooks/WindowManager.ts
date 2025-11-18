import * as loader from "tc-loader";
import {setWindowManager} from "tc-shared/ui/windows/WindowManager";
import {NativeWindowManager} from "../WindowManager";

loader.register_task(loader.Stage.JAVASCRIPT_INITIALIZING, {
    name: "window manager initialize",
    function: async () => setWindowManager(new NativeWindowManager()),
    priority: 100
});