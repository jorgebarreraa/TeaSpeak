import {
    AbstractVoiceConnection,
    VoiceConnectionStatus,
    WhisperSessionInitializer
} from "tc-shared/connection/VoiceConnection";
import {ConnectionRecorderProfileOwner, RecorderProfile} from "tc-shared/voice/RecorderProfile";
import {NativeServerConnection, NativeVoiceClient, NativeVoiceConnection, PlayerState} from "tc-native/connection";
import {ServerConnection} from "./ServerConnection";
import {VoiceClient} from "tc-shared/voice/VoiceClient";
import {WhisperSession, WhisperTarget} from "tc-shared/voice/VoiceWhisper";
import {NativeInput} from "../audio/AudioRecorder";
import {ConnectionHandler, ConnectionState} from "tc-shared/ConnectionHandler";
import {VoicePlayerEvents, VoicePlayerLatencySettings, VoicePlayerState} from "tc-shared/voice/VoicePlayer";
import {Registry} from "tc-shared/events";
import {LogCategory, logError, logInfo, logWarn} from "tc-shared/log";
import {tr} from "tc-shared/i18n/localize";
import {ConnectionStatistics} from "tc-shared/connection/ConnectionBase";
import {AbstractInput} from "tc-shared/voice/RecorderBase";
import {crashOnThrow, ignorePromise} from "tc-shared/proto";

export class NativeVoiceConnectionWrapper extends AbstractVoiceConnection {
    private readonly serverConnectionStateChangedListener;
    private readonly native: NativeVoiceConnection;

    private localAudioStarted = false;
    private connectionState: VoiceConnectionStatus;
    private currentRecorder: RecorderProfile;

    private ignoreRecorderUnmount: boolean;
    private listenerRecorder: (() => void)[];

    private registeredVoiceClients: {[key: number]: NativeVoiceClientWrapper} = {};

    private currentlyReplayingAudio = false;
    private readonly voiceClientStateChangedEventListener;

    constructor(connection: ServerConnection, voice: NativeVoiceConnection) {
        super(connection);
        this.native = voice;
        this.ignoreRecorderUnmount = false;

        this.serverConnectionStateChangedListener = () => {
            if(this.connection.getConnectionState() === ConnectionState.CONNECTED) {
                this.setConnectionState(VoiceConnectionStatus.Connected);
            } else {
                this.setConnectionState(VoiceConnectionStatus.Disconnected);
            }
        }

        this.connection.events.on("notify_connection_state_changed", this.serverConnectionStateChangedListener);
        this.connectionState = VoiceConnectionStatus.Disconnected;

        this.voiceClientStateChangedEventListener = this.handleVoiceClientStateChange.bind(this);
    }

    destroy() {
        this.connection.events.off("notify_connection_state_changed", this.serverConnectionStateChangedListener);
    }

    getConnectionState(): VoiceConnectionStatus {
        return this.connectionState;
    }

    getFailedMessage(): string {
        /* the native voice connection can't fail */
        return "this message should never appear";
    }

    private setConnectionState(state: VoiceConnectionStatus) {
        if(this.connectionState === state) {
            return;
        }

        const oldState = this.connectionState;
        this.connectionState = state;
        this.events.fire("notify_connection_status_changed", { oldStatus: oldState, newStatus: state });
    }

    encodingSupported(codec: number): boolean {
        return this.native.encoding_supported(codec);
    }

    decodingSupported(codec: number): boolean {
        return this.native.decoding_supported(codec);
    }

    async acquireVoiceRecorder(recorder: RecorderProfile | undefined, enforce?: boolean): Promise<void> {
        if(this.currentRecorder === recorder && !enforce) {
            return;
        }

        this.listenerRecorder?.forEach(callback => callback());
        this.listenerRecorder = undefined;

        if(this.currentRecorder) {
            this.ignoreRecorderUnmount = true;
            this.ignoreRecorderUnmount = false;

            this.native.set_audio_source(undefined);
        }

        const oldRecorder = recorder;
        this.currentRecorder = recorder;

        if(this.currentRecorder) {
            const connection = this;
            await recorder.ownRecorder(new class extends ConnectionRecorderProfileOwner {
                getConnection(): ConnectionHandler {
                    return connection.connection.client;
                }

                protected handleRecorderInput(input: AbstractInput): any {
                    if(!(input instanceof NativeInput)) {
                        logError(LogCategory.VOICE, tr("Recorder input isn't an instance of NativeInput. Ignoring recorder input."));
                        return;
                    }

                    connection.native.set_audio_source(input.getNativeConsumer());
                }

                protected handleUnmount(): any {
                    if(connection.ignoreRecorderUnmount) {
                        return;
                    }

                    connection.currentRecorder = undefined;
                    ignorePromise(crashOnThrow(connection.acquireVoiceRecorder(undefined, true)));
                }
            });

            this.listenerRecorder = [];
            this.listenerRecorder.push(recorder.events.on("notify_voice_start", () => this.handleVoiceStartEvent()));
            this.listenerRecorder.push(recorder.events.on("notify_voice_end", () => this.handleVoiceEndEvent(tr("recorder event"))));
        }

        if(this.currentRecorder?.isInputActive()) {
            this.handleVoiceStartEvent();
        } else {
            this.handleVoiceEndEvent(tr("recorder change"));
        }

        this.events.fire("notify_recorder_changed", {
            oldRecorder,
            newRecorder: recorder
        });
    }

    voiceRecorder(): RecorderProfile {
        return this.currentRecorder;
    }

    getEncoderCodec(): number {
        return this.native.get_encoder_codec();
    }

    setEncoderCodec(codec: number) {
        this.native.set_encoder_codec(codec);
    }

    isReplayingVoice(): boolean {
        return this.currentlyReplayingAudio;
    }

    private setReplayingVoice(status: boolean) {
        if(status === this.currentlyReplayingAudio) {
            return;
        }

        this.currentlyReplayingAudio = status;
        this.events.fire("notify_voice_replay_state_change", { replaying: status });
    }

    private handleVoiceClientStateChange() {
        this.setReplayingVoice(this.availableVoiceClients().findIndex(client => client.getState() === VoicePlayerState.PLAYING || client.getState() === VoicePlayerState.BUFFERING) !== -1);
    }

    private handleVoiceStartEvent() {
        const chandler = this.connection.client;
        if(chandler.isMicrophoneMuted()) {
            logWarn(LogCategory.VOICE, tr("Received local voice started event, even thou we're muted!"));
            return;
        }

        this.native.enable_voice_send(true);
        this.localAudioStarted = true;

        logInfo(LogCategory.VOICE, tr("Local voice started"));
        chandler.getClient()?.setSpeaking(true);
    }

    private handleVoiceEndEvent(reason: string) {
        this.native.enable_voice_send(false);

        if(!this.localAudioStarted) {
            return;
        }

        const chandler = this.connection.client;
        chandler.getClient()?.setSpeaking(false);

        logInfo(LogCategory.VOICE, tr("Local voice ended (%s)"), reason);
        this.localAudioStarted = false;
    }

    availableVoiceClients(): NativeVoiceClientWrapper[] {
        return Object.keys(this.registeredVoiceClients).map(clientId => this.registeredVoiceClients[clientId]);
    }

    registerVoiceClient(clientId: number) {
        const client = new NativeVoiceClientWrapper(this.native.register_client(clientId));
        client.events.on("notify_state_changed", this.voiceClientStateChangedEventListener);
        this.registeredVoiceClients[clientId] = client;
        return client;
    }

    unregisterVoiceClient(client: VoiceClient) {
        if(!(client instanceof NativeVoiceClientWrapper)) {
            throw "invalid client type";
        }

        delete this.registeredVoiceClients[client.getClientId()];
        this.native.unregister_client(client.getClientId());
        client.destroy();

        this.handleVoiceClientStateChange();
    }

    stopAllVoiceReplays() {
        this.availableVoiceClients().forEach(client => client.abortReplay());
    }

    /* whisper API */
    getWhisperSessionInitializer(): WhisperSessionInitializer | undefined {
        return undefined;
    }

    getWhisperSessions(): WhisperSession[] {
        return [];
    }

    getWhisperTarget(): WhisperTarget | undefined {
        return undefined;
    }

    setWhisperSessionInitializer(initializer: WhisperSessionInitializer | undefined) { }

    startWhisper(target: WhisperTarget): Promise<void> {
        return Promise.resolve(undefined);
    }

    dropWhisperSession(session: WhisperSession) { }

    stopWhisper() { }

    getConnectionStats(): Promise<ConnectionStatistics> {
        /* FIXME: This is iffy! */
        const stats = (this.connection as any as NativeServerConnection)["nativeHandle"]?.statistics();

        return Promise.resolve({
            bytesSend: stats?.voice_bytes_send ? stats?.voice_bytes_send : 0,
            bytesReceived: stats?.voice_bytes_received ? stats?.voice_bytes_received : 0
        });
    }

    getRetryTimestamp(): number | 0 {
        return Date.now();
    }
}

class NativeVoiceClientWrapper implements VoiceClient {
    private readonly native: NativeVoiceClient;
    readonly events: Registry<VoicePlayerEvents>;
    private playerState: VoicePlayerState;

    constructor(native: NativeVoiceClient) {
        this.events = new Registry<VoicePlayerEvents>();
        this.native = native;
        this.playerState = VoicePlayerState.STOPPED;

        this.native.callback_state_changed = state => {
            switch (state) {
                case PlayerState.BUFFERING:
                    this.setState(VoicePlayerState.BUFFERING);
                    break;

                case PlayerState.PLAYING:
                    this.setState(VoicePlayerState.PLAYING);
                    break;

                case PlayerState.STOPPED:
                    this.setState(VoicePlayerState.STOPPED);
                    break;

                case PlayerState.STOPPING:
                    this.setState(VoicePlayerState.STOPPING);
                    break;

                default:
                    logError(LogCategory.VOICE, tr("Native audio player has invalid state: %o"), state);
                    break;
            }
        }

        this.resetLatencySettings();
    }

    destroy() {
        this.events.destroy();
    }

    abortReplay() {
        this.native.abort_replay();
    }

    flushBuffer() {
        this.native.get_stream().flush_buffer();
    }

    getClientId(): number {
        return this.native.client_id;
    }

    getState(): VoicePlayerState {
        return this.playerState;
    }

    private setState(state: VoicePlayerState) {
        if(this.playerState === state) {
            return;
        }

        const oldState = this.playerState;
        this.playerState = state;
        this.events.fire("notify_state_changed", { oldState: oldState, newState: state });
    }

    setVolume(volume: number) {
        this.native.set_volume(volume);
    }

    getVolume(): number {
        return this.native.get_volume();
    }

    resetLatencySettings() {
        const stream = this.native.get_stream();
        stream.set_buffer_latency(0.080);
        stream.set_buffer_max_latency(0.5);
    }

    setLatencySettings(settings: VoicePlayerLatencySettings) {
        const stream = this.native.get_stream();
        stream.set_buffer_latency(settings.minBufferTime / 1000);
        stream.set_buffer_max_latency(settings.maxBufferTime / 1000);
    }

    getLatencySettings(): Readonly<VoicePlayerLatencySettings> {
        const stream = this.native.get_stream();

        return {
            maxBufferTime: stream.get_buffer_max_latency() * 1000,
            minBufferTime: stream.get_buffer_latency() * 1000
        };
    }
}