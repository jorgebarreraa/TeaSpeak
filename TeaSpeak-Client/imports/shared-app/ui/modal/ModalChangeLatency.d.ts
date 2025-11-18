import { ClientEntry } from "../../tree/Client";
import { VoicePlayerLatencySettings } from "../../voice/VoicePlayer";
export declare function spawnChangeLatency(client: ClientEntry, current: VoicePlayerLatencySettings, reset: () => VoicePlayerLatencySettings, apply: (settings: VoicePlayerLatencySettings) => void, callback_flush?: () => any): void;
