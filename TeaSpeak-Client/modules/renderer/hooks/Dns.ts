import * as loader from "tc-loader";
import {setDNSProvider} from "tc-shared/dns";
import {NativeDnsResolver} from "../dns/NativeDnsResolver";

loader.register_task(loader.Stage.JAVASCRIPT_INITIALIZING, {
    name: "Native DNS initialized",
    function: async () => setDNSProvider(new NativeDnsResolver()),
    priority: 100
});