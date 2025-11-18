export declare type Entry = {
    timestamp: number;
    upload?: number;
    download?: number;
    highlight?: boolean;
};
export declare type Style = {
    backgroundColor: string;
    separatorColor: string;
    separatorCount: number;
    separatorWidth: number;
    upload: {
        fill: string;
        stroke: string;
        strokeWidth: number;
    };
    download: {
        fill: string;
        stroke: string;
        strokeWidth: number;
    };
};
export declare type TimeSpan = {
    origin: {
        begin: number;
        end: number;
        time: number;
    };
    target: {
        begin: number;
        end: number;
        time: number;
    };
};
export declare class Graph {
    private static animateCallbacks;
    private static registerAnimateCallback;
    private static removerAnimateCallback;
    style: Style;
    private canvas;
    private canvasContext;
    private entries;
    private entriesMax;
    private maxSpace;
    private maxGap;
    private animateLoop;
    timeSpan: TimeSpan;
    private detailShown;
    callbackDetailedInfo: (upload: number, download: number, timestamp: number, event: MouseEvent) => any;
    callbackDetailedHide: () => any;
    constructor();
    initialize(): void;
    finalize(): void;
    initializeCanvas(canvas: HTMLCanvasElement | undefined): void;
    maxGapSize(value?: number): number;
    private recalculateCache;
    entryCount(): number;
    pushEntry(entry: Entry): void;
    insertEntries(entries: Entry[]): void;
    resize(): void;
    cleanup(): void;
    calculateTimespan(): {
        begin: number;
        end: number;
    };
    private draw;
    private onMouseMove;
    private onMouseLeave;
}
