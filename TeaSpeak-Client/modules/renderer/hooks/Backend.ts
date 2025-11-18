import {setBackend} from "tc-shared/backend";
import {NativeClientBackendImpl} from "../Backend";

setBackend(new NativeClientBackendImpl());