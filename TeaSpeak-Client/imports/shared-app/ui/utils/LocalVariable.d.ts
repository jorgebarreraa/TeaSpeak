import { UiVariableConsumer, UiVariableMap, UiVariableProvider } from "tc-shared/ui/utils/Variable";
export declare function createLocalUiVariables<Variables extends UiVariableMap>(): [UiVariableProvider<Variables>, UiVariableConsumer<Variables>];
