export declare class Mutex<T> {
    private value;
    private taskExecuting;
    private taskQueue;
    private freeListener;
    constructor(value: T);
    isFree(): boolean;
    awaitFree(): Promise<void>;
    execute<R>(callback: (value: T, setValue: (newValue: T) => void) => R | Promise<R>): Promise<R>;
    tryExecute<R>(callback: (value: T, setValue: (newValue: T) => void) => R | Promise<R>): Promise<{
        status: "success";
        result: R;
    } | {
        status: "would-block";
    }>;
    private executeNextTask;
    private triggerFinished;
}
