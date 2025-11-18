import * as loader from "tc-loader";
import {setSoundBackend} from "tc-shared/audio/Sounds";
import {NativeSoundBackend} from "../audio/Sounds";

loader.register_task(loader.Stage.JAVASCRIPT_INITIALIZING, {
    name: "Native sound initialized",
    function: async () => setSoundBackend(new NativeSoundBackend()),
    priority: 100
});