declare module "teaclient_crash_handler" {
    export function setup_crash_handler(
        component_name: string,
        crash_dump_folder: string,
        success_command_line: string, /* %crash_path% for crash dump path */
        error_command_line: string /* %error_message% for the error message */
    );
    export function finalize();
    export function crash_handler_active() : boolean;

    export function crash();
}