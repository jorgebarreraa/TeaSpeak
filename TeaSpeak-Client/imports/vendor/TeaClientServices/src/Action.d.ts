import { MessageCommandErrorResult } from "./Messages";
export declare type ActionResult<T> = {
    unwrap(): T;
} & ({
    status: "success";
    result: T;
} | {
    status: "error";
    result: MessageCommandErrorResult;
});
export declare function createErrorResult<T>(result: MessageCommandErrorResult): ActionResult<T>;
export declare function createResult(): ActionResult<void>;
export declare function createResult<T>(result: T): ActionResult<T>;
