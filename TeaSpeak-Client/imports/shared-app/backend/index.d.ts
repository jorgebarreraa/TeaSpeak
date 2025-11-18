import { NativeClientBackend } from "tc-shared/backend/NativeClient";
import { WebClientBackend } from "tc-shared/backend/WebClient";
export declare function getBackend(target: "native"): NativeClientBackend;
export declare function getBackend(target: "web"): WebClientBackend;
export declare function setBackend(instance: NativeClientBackend | WebClientBackend): void;
