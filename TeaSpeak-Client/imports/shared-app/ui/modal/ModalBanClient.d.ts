import { ConnectionHandler } from "../../ConnectionHandler";
export declare type BanEntry = {
    name?: string;
    unique_id: string;
};
export declare function spawnBanClient(client: ConnectionHandler, entries: BanEntry | BanEntry[], callback: (data: {
    length: number;
    reason: string;
    no_name: boolean;
    no_ip: boolean;
    no_hwid: boolean;
}) => void): void;
