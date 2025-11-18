import { Dispatch, SetStateAction } from "react";
import { RegistryKey, RegistryValueType, ValuedRegistryKey } from "tc-shared/settings";
export declare function useDependentState<S>(factory: (prevState?: S) => S, inputs: ReadonlyArray<any>): [S, Dispatch<SetStateAction<S>>];
export declare function useTr(message: string): string;
export declare function joinClassList(...classes: any[]): string;
export declare function useGlobalSetting<V extends RegistryValueType>(key: ValuedRegistryKey<V>, defaultValue?: V): V;
export declare function useGlobalSetting<V extends RegistryValueType, DV>(key: RegistryKey<V>, defaultValue: DV): V | DV;
