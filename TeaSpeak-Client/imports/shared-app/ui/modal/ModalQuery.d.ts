import { ConnectionHandler } from "../../ConnectionHandler";
export declare function spawnQueryCreate(connection: ConnectionHandler, callback_created?: (user: any, pass: any) => any): void;
export declare function spawnQueryCreated(credentials: {
    username: string;
    password: string;
}, just_created: boolean): void;
