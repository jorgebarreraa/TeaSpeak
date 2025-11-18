import * as loader from "tc-loader";
import {setKeyBoardBackend} from "tc-shared/PPTListener";
import {NativeKeyBoard} from "../KeyBoard";

loader.register_task(loader.Stage.JAVASCRIPT_INITIALIZING, {
    name: "Native keyboard initialized",
    function: async () => setKeyBoardBackend(new NativeKeyBoard()),
    priority: 100
});