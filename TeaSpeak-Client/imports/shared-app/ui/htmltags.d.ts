export interface ClientProperties {
    client_id: number;
    client_unique_id: string;
    client_name: string;
    add_braces?: boolean;
    client_database_id?: number;
}
export declare function generate_client(properties: ClientProperties): string;
export declare function generate_client_object(properties: ClientProperties): JQuery;
export declare namespace callbacks {
    function callback_context_client(element: JQuery): boolean;
}
