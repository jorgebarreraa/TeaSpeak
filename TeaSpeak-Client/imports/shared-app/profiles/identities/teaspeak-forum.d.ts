declare global {
    interface Window {
        grecaptcha: GReCaptcha;
    }
}
interface GReCaptcha {
    render(container: string | HTMLElement, parameters: {
        sitekey: string;
        theme?: "dark" | "light";
        size?: "compact" | "normal";
        tabindex?: number;
        callback?: (token: string) => any;
        "expired-callback"?: () => any;
        "error-callback"?: (error: any) => any;
    }): string;
    reset(widget_id?: string): any;
}
export declare namespace gcaptcha {
    function initialize(): Promise<void>;
    function spawn(container: JQuery, key: string, callback_data: (token: string) => any): Promise<void>;
}
export declare class Data {
    readonly auth_key: string;
    readonly raw: string;
    readonly sign: string;
    parsed: {
        user_id: number;
        user_name: string;
        data_age: number;
        user_group_id: number;
        is_staff: boolean;
        user_groups: number[];
    };
    constructor(auth: string, raw: string, sign: string);
    data_json(): string;
    data_sign(): string;
    name(): string;
    user_id(): number;
    user_group(): number;
    is_stuff(): boolean;
    is_premium(): boolean;
    data_age(): Date;
    is_expired(): boolean;
    should_renew(): boolean;
}
export declare function logged_in(): boolean;
export declare function data(): Data;
export interface LoginResult {
    status: "success" | "captcha" | "error";
    error_message?: string;
    captcha?: {
        type: "gre-captcha" | "unknown";
        data: any;
    };
}
export declare function login(username: string, password: string, captcha?: any): Promise<LoginResult>;
export declare function renew_data(): Promise<"success" | "login-required">;
export declare function logout(): Promise<void>;
export {};
