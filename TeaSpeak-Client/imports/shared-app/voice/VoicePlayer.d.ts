import { Registry } from "../events";
export declare enum VoicePlayerState {
    INITIALIZING = 0,
    PREBUFFERING = 1,
    PLAYING = 2,
    BUFFERING = 3,
    STOPPING = 4,
    STOPPED = 5
}
export interface VoicePlayerEvents {
    notify_state_changed: {
        oldState: VoicePlayerState;
        newState: VoicePlayerState;
    };
}
export interface VoicePlayerLatencySettings {
    minBufferTime: number;
    maxBufferTime: number;
}
export interface VoicePlayer {
    readonly events: Registry<VoicePlayerEvents>;
    /**
     * @returns Returns the current voice player state.
     *          Subscribe to the "notify_state_changed" event to receive player changes.
     */
    getState(): VoicePlayerState;
    /**
     * @returns The volume multiplier in a range from [0, 1]
     */
    getVolume(): number;
    /**
     * @param volume The volume multiplier in a range from [0, 1]
     */
    setVolume(volume: number): any;
    /**
     * Abort the replaying of the currently pending buffers.
     * If new buffers are arriving a new replay will be started.
     */
    abortReplay(): any;
    /**
     * Flush the current buffer.
     * This will most likely set the player into the buffering mode.
     */
    flushBuffer(): any;
    /**
     * Get the currently used latency settings
     */
    getLatencySettings(): Readonly<VoicePlayerLatencySettings>;
    /**
     * @param settings The new latency settings to be used
     */
    setLatencySettings(settings: VoicePlayerLatencySettings): any;
    /**
     * Reset the latency settings to the default
     */
    resetLatencySettings(): any;
}
