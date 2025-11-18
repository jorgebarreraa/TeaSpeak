import {AbstractCommandHandler, AbstractCommandHandlerBoss} from "tc-shared/connection/AbstractCommandHandler";
import {
    AbstractServerConnection,
    CommandOptionDefaults,
    CommandOptions, ConnectionPing,
    ConnectionStatistics,
    ServerCommand
} from "tc-shared/connection/ConnectionBase";
import {CommandResult} from "tc-shared/connection/ServerConnectionDeclaration";
import {tr} from "tc-shared/i18n/localize";
import {ConnectionHandler, ConnectionState, DisconnectReason} from "tc-shared/ConnectionHandler";
import {
    destroy_server_connection as destroy_native_server_connection,
    NativeServerConnection,
    ServerType,
    spawn_server_connection as spawn_native_server_connection
} from "tc-native/connection";
import {ConnectionCommandHandler} from "tc-shared/connection/CommandHandler";
import {HandshakeHandler} from "tc-shared/connection/HandshakeHandler";
import {TeaSpeakHandshakeHandler} from "tc-shared/profiles/identities/TeamSpeakIdentity";
import {NativeVoiceConnectionWrapper} from "./VoiceConnection";
import {AbstractVoiceConnection} from "tc-shared/connection/VoiceConnection";
import {LogCategory, logDebug, logWarn} from "tc-shared/log";
import {ErrorCode} from "tc-shared/connection/ErrorCode";
import {ServerAddress} from "tc-shared/tree/Server";
import {VideoConnection} from "tc-shared/connection/VideoConnection";
import {RTCConnection} from "tc-shared/connection/rtc/Connection";
import {RtpVideoConnection} from "tc-shared/connection/rtc/video/Connection";

interface ErrorCodeListener {
    callback: (result: CommandResult) => void;
    code: string;

    command: string;
    timeout: number;
}

class ErrorCommandHandler extends AbstractCommandHandler {
    private readonly handle: ServerConnection;

    private errorCodeMapping: {[key: string]: ErrorCodeListener} = {};
    private errorCodeHistory: ErrorCodeListener[] = [];

    private errorCodeIndex = 0;

    constructor(handle: ServerConnection) {
        super(handle);
        this.handle = handle;
    }

    generateReturnCode(command: string, callback: (result: CommandResult) => void, returnCode?: string) : string {
        if(typeof returnCode === "undefined") {
            returnCode = "rt-" + (++this.errorCodeIndex);
        }

        const listener = {
            callback: callback,
            code: returnCode,
            timeout: 0,
            command: command
        } as ErrorCodeListener;

        listener.timeout = setTimeout(() => {
            delete this.errorCodeMapping[listener.code];
            const index = this.errorCodeHistory.indexOf(listener);
            if(index !== -1) {
                this.errorCodeHistory.splice(index, 1);
            }

            logWarn(LogCategory.NETWORKING, tr("Command %s timeout out."), command);
            callback(new CommandResult([{ id: ErrorCode.COMMAND_TIMED_OUT, msg: "timeout" }]));
        }, 5000) as any;

        this.errorCodeMapping[listener.code] = listener;
        this.errorCodeHistory.push(listener);

        return listener.code;
    }

    handle_command(command: ServerCommand): boolean {
        if(command.command === "error") {
            const data = command.arguments[0];

            const returnCode = data["return_code"];
            let codeListener: ErrorCodeListener;

            if(!returnCode) {
                const [ code ] = this.errorCodeHistory.splice(0, 1);
                if(!code) {
                    logWarn(LogCategory.NETWORKING, tr("Received error without a return code and we're not expecting an error."));
                    return true;
                }
                logDebug(LogCategory.NETWORKING, tr("Received error without any error code. Using the first send command %s (%s)"), code.command, code.code);

                codeListener = code;
            } else {
                let code = this.errorCodeMapping[returnCode];
                if(!code) {
                    logWarn(LogCategory.NETWORKING, tr("Received error for invalid return code %s"), returnCode);
                    return true;
                }

                const index = this.errorCodeHistory.indexOf(code);
                if(index !== -1) this.errorCodeHistory.splice(index, 1);

                codeListener = code;
            }

            delete this.errorCodeMapping[codeListener.code];
            clearTimeout(codeListener.timeout);

            codeListener.callback(new CommandResult(command.arguments));
            return true;
        } else if(command.command == "initivexpand") {
            if(command.arguments[0]["teaspeak"] == true) {
                this.handle.handshake_handler().startHandshake();
                this.handle["serverType"] = "teaspeak";
            } else {
                this.handle["serverType"] = "teamspeak";
            }
            return true;
        } else if(command.command == "initivexpand2") {
            /* its TeamSpeak or TeaSpeak with experimental 3.1 and not up2date */
            this.handle["serverType"] = "teamspeak";
        } else if(command.command == "initserver") {
            /* just if clientinit error did not fired (TeamSpeak) */
            while(this.errorCodeHistory.length > 0) {
                const listener = this.errorCodeHistory.pop();
                listener.callback(new CommandResult([{id: 0, message: ""}]));
                clearTimeout(listener.timeout);
            }

            if(this.handle.getServerType() === "teaspeak") {
                this.handle.getRtcConnection().doInitialSetup();
            } else {
                this.handle.getRtcConnection().setNotSupported();
            }
            this.errorCodeMapping = {};
        } else if(command.command == "notifyconnectioninforequest") {
            this.handle.send_command("setconnectioninfo",
                {
                    //TODO calculate
                    connection_ping: 0.0000,
                    connection_ping_deviation: 0.0,

                    connection_packets_sent_speech: 0,
                    connection_packets_sent_keepalive: 0,
                    connection_packets_sent_control: 0,
                    connection_bytes_sent_speech: 0,
                    connection_bytes_sent_keepalive: 0,
                    connection_bytes_sent_control: 0,
                    connection_packets_received_speech: 0,
                    connection_packets_received_keepalive: 0,
                    connection_packets_received_control: 0,
                    connection_bytes_received_speech: 0,
                    connection_bytes_received_keepalive: 0,
                    connection_bytes_received_control: 0,
                    connection_server2client_packetloss_speech: 0.0000,
                    connection_server2client_packetloss_keepalive: 0.0000,
                    connection_server2client_packetloss_control: 0.0000,
                    connection_server2client_packetloss_total: 0.0000,
                    connection_bandwidth_sent_last_second_speech: 0,
                    connection_bandwidth_sent_last_second_keepalive: 0,
                    connection_bandwidth_sent_last_second_control: 0,
                    connection_bandwidth_sent_last_minute_speech: 0,
                    connection_bandwidth_sent_last_minute_keepalive: 0,
                    connection_bandwidth_sent_last_minute_control: 0,
                    connection_bandwidth_received_last_second_speech: 0,
                    connection_bandwidth_received_last_second_keepalive: 0,
                    connection_bandwidth_received_last_second_control: 0,
                    connection_bandwidth_received_last_minute_speech: 0,
                    connection_bandwidth_received_last_minute_keepalive: 0,
                    connection_bandwidth_received_last_minute_control: 0
                }, { process_result: false }
            );
        }
        return false;
    }
}

export class ServerConnection extends AbstractServerConnection {
    private nativeHandle: NativeServerConnection;

    private readonly rtcConnection: RTCConnection;
    private readonly voiceConnection: NativeVoiceConnectionWrapper;
    private readonly videoConnection: VideoConnection;

    private connectTeamSpeak: boolean;

    private readonly commandHandler: NativeConnectionCommandBoss;
    private readonly commandErrorHandler: ErrorCommandHandler;
    private readonly defaultCommandHandler: ConnectionCommandHandler;

    private remoteAddress: ServerAddress;
    private handshakeHandler: HandshakeHandler;

    private serverType: "teaspeak" | "teamspeak";

    constructor(props: ConnectionHandler) {
        super(props);

        this.commandHandler = new NativeConnectionCommandBoss(this);
        this.commandErrorHandler = new ErrorCommandHandler(this);
        this.defaultCommandHandler = new ConnectionCommandHandler(this);

        this.rtcConnection = new RTCConnection(this, false);
        this.videoConnection = new RtpVideoConnection(this.rtcConnection);

        this.commandHandler.registerHandler(this.commandErrorHandler);
        this.commandHandler.registerHandler(this.defaultCommandHandler);

        this.nativeHandle = spawn_native_server_connection();
        this.nativeHandle.callback_disconnect = reason => {
            switch (this.connectionState) {
                case ConnectionState.CONNECTING:
                case ConnectionState.AUTHENTICATING:
                case ConnectionState.INITIALISING:
                    this.client.handleDisconnect(DisconnectReason.CONNECT_FAILURE, reason);
                    break;

                case ConnectionState.CONNECTED:
                    this.client.handleDisconnect(DisconnectReason.CONNECTION_CLOSED, {
                        reason: reason
                    });
                    break;

                case ConnectionState.DISCONNECTING:
                case ConnectionState.UNCONNECTED:
                    break;
            }
        };
        this.nativeHandle.callback_command = (command, args, switches) => {
            console.log("Received: %o %o %o", command, args, switches);
            //FIXME catch error

            this.commandHandler.invokeCommand(new ServerCommand(command, args, switches));
        };

        this.voiceConnection = new NativeVoiceConnectionWrapper(this, this.nativeHandle._voice_connection);

        this.command_helper.initialize();
    }

    native_handle() : NativeServerConnection {
        return this.nativeHandle;
    }

    finalize() {
        if(this.nativeHandle) {
            if(destroy_native_server_connection) {
                /* currently not defined but may will be ;) */
                destroy_native_server_connection(this.nativeHandle);
            }
            this.nativeHandle = undefined;
        }

        this.rtcConnection.destroy();
    }

    connect(address: ServerAddress, handshake: HandshakeHandler, timeout?: number): Promise<void> {
        this.updateConnectionState(ConnectionState.CONNECTING);

        this.remoteAddress = address;
        this.handshakeHandler = handshake;
        this.connectTeamSpeak = false;
        handshake.setConnection(this);
        handshake.initialize();

        return new Promise<void>((resolve, reject) => {
            this.nativeHandle.connect({
                remote_host: address.host,
                remote_port: address.port,

                timeout: typeof(timeout) === "number" ? timeout : -1,


                callback: error => {
                    if(error != 0) {
                        /* required to notify the handle, just a promise reject does not work */
                        this.client.handleDisconnect(DisconnectReason.CONNECT_FAILURE, error);
                        this.updateConnectionState(ConnectionState.UNCONNECTED);
                        reject(this.nativeHandle.error_message(error));
                        return;
                    } else {
                        resolve();
                    }
                    this.updateConnectionState(ConnectionState.AUTHENTICATING);

                    console.log("Remote server type: %o (%s)", this.nativeHandle.server_type, ServerType[this.nativeHandle.server_type]);
                    if(this.nativeHandle.server_type == ServerType.TEAMSPEAK || this.connectTeamSpeak) {
                        console.log("Trying to use TeamSpeak's identity system");
                        this.handshake_handler().on_teamspeak();
                    }
                },

                identity_key: (handshake.get_identity_handler() as TeaSpeakHandshakeHandler).identity.private_key,
                teamspeak: false
            })
        });
    }


    remote_address(): ServerAddress {
        return this.remoteAddress;
    }

    handshake_handler(): HandshakeHandler {
        return this.handshakeHandler;
    }

    connected(): boolean {
        return typeof(this.nativeHandle) !== "undefined" && this.nativeHandle.connected();
    }

    disconnect(reason?: string): Promise<void> {
        console.trace("Disconnect: %s",reason);
        return new Promise<void>((resolve, reject) => this.nativeHandle.disconnect(reason || "", error => {
            if(error == 0)
                resolve();
            else
                reject(this.nativeHandle.error_message(error));
        }));
    }

    support_voice(): boolean {
        return true;
    }

    getVoiceConnection(): AbstractVoiceConnection {
        return this.voiceConnection;
    }

    getCommandHandler(): AbstractCommandHandlerBoss {
        return this.commandHandler;
    }

    send_command(command: string, data?: any, _options?: CommandOptions): Promise<CommandResult> {
        if(!this.connected()) {
            console.warn(tr("Tried to send a command without a valid connection."));
            return Promise.reject(tr("not connected"));
        }

        const options: CommandOptions = {};
        Object.assign(options, CommandOptionDefaults);
        Object.assign(options, _options);

        data = Array.isArray(data) ? data : [data || {}];
        if(data.length == 0) { /* we require min one arg to append return_code */
            data.push({});
        }

        console.log("Send: %o %o", command, data);
        const promise = new Promise<CommandResult>((resolve, reject) => {
            data[0]["return_code"] = this.commandErrorHandler.generateReturnCode(command, result => {
                if(result.success) {
                    resolve(result);
                } else {
                    reject(result);
                }
            }, data[0]["return_code"]);

            try {
                this.nativeHandle.send_command(command, data, options.flagset || []);
            } catch(error) {
                reject(tr("failed to send command"));
                console.warn(tr("Failed to send command: %o"), error);
            }
        });
        return this.defaultCommandHandler.proxy_command_promise(promise, options);
    }

    ping(): ConnectionPing {
        return {
            native: this.nativeHandle ? (this.nativeHandle.current_ping() / 1000) : -2,
            javascript: undefined
        };
    }

    getControlStatistics(): ConnectionStatistics {
        const stats = this.nativeHandle?.statistics();

        return {
            bytesReceived: stats?.control_bytes_received ? stats?.control_bytes_received : 0,
            bytesSend: stats?.control_bytes_send ? stats?.control_bytes_send : 0
        };
    }

    getVideoConnection(): VideoConnection {
        return this.videoConnection;
    }

    getRtcConnection() : RTCConnection {
        return this.rtcConnection;
    }


    getServerType() : "teaspeak" | "teamspeak" | "unknown" {
        return this.serverType;
    }
}

export class NativeConnectionCommandBoss extends AbstractCommandHandlerBoss {
    constructor(connection: AbstractServerConnection) {
        super(connection);
    }
}
