import * as loader from "tc-loader";
import {setAudioBackend} from "tc-shared/audio/Player";
import {NativeAudioPlayer} from "../audio/AudioPlayer";

loader.register_task(loader.Stage.JAVASCRIPT_INITIALIZING, {
    name: "Native audi backend initialized",
    function: async () => setAudioBackend(new NativeAudioPlayer()),
    priority: 100
});