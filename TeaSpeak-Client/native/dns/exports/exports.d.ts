declare module "tc-native/dns" {
    export function resolve_cr(host: string, port: number, callback: (result: string | {host: string, port: number}) => any);
    export function initialize();
}