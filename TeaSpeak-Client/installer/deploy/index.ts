import * as fs from "fs";
import * as path from "path";
import * as _node_ssh from "node-ssh";
import * as ssh2 from "ssh2";
import * as stream from "stream";
import * as child_process from "child_process";
import * as util from "util";
import {FileEntry} from "ssh2-streams";

declare namespace node_ssh {
    export type PutFilesOptions = {
        sftp?: Object,
        sftpOptions?: Object,
        concurrency?: number,
    }
    export type PutDirectoryOptions = {
        sftp?: Object,
        sftpOptions?: Object,
        concurrency?: number,
        recursive?: boolean,
        tick?: ((localPath: string, remotePath: string, error?: Error) => void),
        validate?: ((localPath: string) => boolean),
    }
    export type ExecOptions = {
        cwd?: string,
        options?: Object // passed to ssh2.exec
        stdin?: string,
        stream?: 'stdout' | 'stderr' | 'both',
        onStdout?: ((chunk: Buffer) => void),
        onStderr?: ((chunk: Buffer) => void),
    }

    export class Instance {
        connect(config: ssh2.ConnectConfig): Promise<this>
        requestSFTP(): Promise<ssh2.SFTPWrapper>
        requestShell(): Promise<ssh2.ClientChannel>
        mkdir(path: string, method: 'sftp' | 'exec', givenSftp?: Object): Promise<string>
        exec(command: string, parameters: Array<string>, options: ExecOptions): Promise<Object | string>
        execCommand(command: string, options?: { cwd: string, stdin: string }): Promise<{ stdout: string, options?: Object, stderr: string, signal?: string, code: number }>
        putFile(localFile: string, remoteFile: string, sftp?: Object, opts?: Object): Promise<void>
        getFile(localFile: string, remoteFile: string, sftp?: Object, opts?: Object): Promise<void>
        putFiles(files: Array<{ local: string, remote: string }>, options: PutFilesOptions): Promise<void>
        putDirectory(localDirectory: string, remoteDirectory: string, options: PutDirectoryOptions): Promise<boolean>
        dispose(): void
    }
}

let instance: node_ssh.Instance;
export async function setup() {
    if(instance) {
        throw "already initiaized";
    }

    instance = new _node_ssh();
    try {
        await instance.connect({
            host: 'deploy.teaspeak.de',
            username: 'TeaSpeak-Jenkins-Client',
            privateKey: path.join(__dirname, "ssh_key")
        })
    } catch(error) {
        try { instance.dispose(); } finally { instance = undefined; }
        console.error("Failed to connect: %o", error);
        throw "failed to connect";
    }
}

function read_stream_full(stream: stream.Readable) : Promise<Buffer> {
    return new Promise((resolve, reject) => {
        const buffers = [];
        stream.on('data', buffer => buffers.push(buffer));
        stream.on('end', () => resolve(Buffer.concat(buffers)));
        stream.on('error', error => reject(error));
    });
}

export type PlatformSpecs = {
    system: 'linux' | 'windows' | 'osx';
    arch: 'amd64' | 'x86';
    type: 'indev' | 'debug' | 'optimized' | 'stable';
}

export type Version = {
    major: number;
    minor: number;
    patch: number;
    type?: 'indev' | 'beta';
}

//<system>/<arch>_<type>/
function platform_path(platform: PlatformSpecs) {
    return platform.system + "/" + platform.arch + "_" + platform.type + "/";
}

function version_string(version: Version) {
    return version.major + "." + version.minor + "." + version.patch + (version.type ? "-" + version.type : "");
}

export async function latest_version(platform: PlatformSpecs) {
    const path = "versions/" + platform_path(platform);
    if(!instance) {
        throw "Invalid instance";
    }

    const sftp = await instance.requestSFTP();
    try {
        if(!sftp)
            throw "failed to request sftp";

        try {
            const data_stream = sftp.createReadStream(path + "latest");
            const data = await read_stream_full(data_stream);
            return data.toString();
        } catch(error) {
            if(error instanceof Error && error.message == "No such file")
                return undefined;

            console.log("Failed to receive last version: %o", error);
            return undefined;
        }
    } finally {
        if(sftp)
            sftp.end();
    }
}

export async function generate_build_index(platform: PlatformSpecs, version: Version) : Promise<number> {
    const path = "versions/" + platform_path(platform);
    const version_str = version_string(version);
    if(!instance)
        throw "Invalid instance";
    const sftp = await instance.requestSFTP();
    try {
        if(!sftp)
            throw "failed to request sftp";

        try {
            const files = await new Promise<FileEntry[]>((resolve, reject) => sftp.readdir(path, (error, result) => error ? reject(error) : resolve(result)));
            const version_files = files.filter(e => e.filename.startsWith(version_str));
            if(version_files.length == 0)
                return 0;
            let index = 1;
            while(version_files.find(e => e.filename.toLowerCase() === version_str + "-" + index)) index++;
            return index;
        } catch(error) {
            if(error instanceof Error && error.message == "No such file")
                return 0;

            console.log("Failed to receive versions list: %o", error);
            return undefined;
        }
    } finally {
        if(sftp)
            sftp.end();
    }
}

export type WinDbgFile = {
    binary: string,
    pdb: string;
};
export async function deploy_win_dbg_files(files: WinDbgFile[], version: Version, path?: string) : Promise<void> {
    //symstore add /r /f .\*.node /s \\deploy.teaspeak.de\symbols /t "TeaClient-Windows-amd64" /v "x.y.z"
    //symstore add /r /f .\*.* /s \\deploy.teaspeak.de\symbols /t "TeaClient-Windows-amd64" /v "1.0.0"
    const server_path = typeof(path) === "string" && path ? path : "\\\\deploy.teaspeak.de\\symbols\\symbols";
    const vstring = version_string(version);
    const exec = util.promisify(child_process.exec);
    for(const file of files) {
        console.log("Deploying %s to %s", file, server_path);
        let current_file;
        try {
            {
                const result = await exec("symstore add /r /f " + file.binary + " /s " + server_path + " /t \"TeaClient-Windows-amd64\" /v \"" + vstring + "\"");
                if(result.stdout)
                    console.log("Stdout: %s", result.stdout);
                if(result.stderr)
                    console.log("Stderr: %s", result.stderr);
            }
            {
                const result = await exec("symstore add /r /f " + file.pdb + " /s " + server_path + " /t \"TeaClient-Windows-amd64\" /v \"" + vstring + "\"");
                if(result.stdout)
                    console.log("Stdout: %s", result.stdout);
                if(result.stderr)
                    console.log("Stderr: %s", result.stderr);
            }
        } catch(error) {
            if('killed' in error && 'code' in error) {
                const perror: {
                    killed: boolean,
                    code: number,
                    signal: any,
                    cmd: string,
                    stdout: string,
                    stderr: string
                } = error;
                console.error("Failed to deploy %s file %s:", current_file, file);
                console.log("  Code: %d", perror.code);
                {
                    console.error("  Stdout: ");
                    for(const element of perror.stdout.split("\n"))
                        console.error("    %s", element);
                }
                {
                    console.error("  Stderr: ");
                    for(const element of perror.stderr.split("\n"))
                        console.error("    %s", element);
                }
            } else
                console.error("Failed to deploy %s file %s: %o", current_file, file, error);
            throw "deploy failed";
        }
    }
}

const test = async () => {
    await setup();
    console.log(await latest_version({
        arch: 'amd64',
        system: 'linux',
        type: 'optimized'
    }));
    console.log(await generate_build_index({
        arch: 'amd64',
        system: 'linux',
        type: 'optimized'
    }, {
        type: 'beta',
        patch: 19,
        minor: 3,
        major: 1
    }));
    /*
    console.log(await deploy_pdb_files(
        [path.join(__dirname, "..", "..", "native", "build", "symbols", "teaclient_crash_handler.pdb")], {
            type: 'beta',
            patch: 19,
            minor: 3,
            major: 1
        }
    ))
    */
};
test();