import * as fs from "fs";
import * as path from "path";
import * as _node_ssh from "node-ssh";
import * as ssh2 from "ssh2";
import * as util from "util";
import * as crypto from "crypto";

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
    if(instance)
        throw "already initiaized";
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
async function update_remote_file(local_path: string, remote_path: string) {
    if(!instance)
        throw "Invalid instance";

    let sftp: ssh2.SFTPWrapper;
    try {
        let sha512_remote, sha512_local;
        try {
            const sha512_remote_result = await instance.execCommand('sha512sum ' + remote_path);
            if(sha512_remote_result.code != 0)
                sha512_remote = undefined; /* file does not exists! */
            else {
                const result = sha512_remote_result.stdout.toString();
                sha512_remote = result.split(" ")[0];
                //console.log("File %s has a remote sha512: %o", remote_path, sha512_remote);
            }
        } catch(error) {
            console.log("Failed to calculate remote sha521 for file %s: %o", remote_path, error);
            return;
        }

        if(sha512_remote) { /* if the remote hasn't the file then we've def a "new" version */
            const hash_processor = crypto.createHash('sha512');
            const local_stream = fs.createReadStream(local_path);

            await new Promise((resolve, reject) => {
                local_stream.on('error', reject);
                local_stream.on('data', chunk => hash_processor.update(chunk));
                local_stream.on('end', resolve);
            });
            sha512_local = hash_processor.digest('hex');
            local_stream.close();
        }

        if(sha512_remote) {
            if(sha512_remote == sha512_local) {
                console.log("File %s (%s) is already up to date.", path.basename(local_path), local_path);
                return;
            } else {
                console.log("Updating file %s (%s) at %s. Local sum: %s Remote sum: %s", path.basename(local_path), local_path, remote_path, sha512_local, sha512_remote);
            }
        } else {
            console.log("Uploading file %s (%s) to %s.", path.basename(local_path), local_path, remote_path);
        }


        try {
            await instance.putFile(local_path, remote_path);
        } catch(error) {
            console.error("Failed to upload file %s (%s): %s", path.basename(local_path), local_path, error);
            throw "Upload failed";
        }
    } finally {
        if(sftp)
            sftp.end();
    }
}

export async function deploy_crash_dumps(local_path: string, remote_path: string) {
    console.log("Uploading crash dumps from %s to %s", local_path, remote_path);

    const do_dir = async (local_path, remote_path) => {
        for(const file of await util.promisify(fs.readdir)(local_path)) {
            const local_file = path.join(local_path, file);
            const remote_file = remote_path + "/" + file;

            if((await util.promisify(fs.stat)(local_file)).isDirectory())
                await do_dir(local_file, remote_file);
            else
                await update_remote_file(local_file, remote_file);
        }
    };

    await do_dir(local_path, remote_path);
}

const test = async () => {
    await setup();
    await deploy_crash_dumps(path.join(__dirname, "../../build/symbols/"), "symbols");
};
test();