import * as React from "react";
import { TypeInfo } from "tc-shared/connectionlog/Definitions";
export declare type RendererEvent<T extends keyof TypeInfo> = (data: TypeInfo[T], handlerId: string, eventType: T) => React.ReactNode;
export declare function findLogEventRenderer<T extends keyof TypeInfo>(type: T): RendererEvent<T>;
export declare function getRegisteredLogEventRenderer(): TypeInfo[];
