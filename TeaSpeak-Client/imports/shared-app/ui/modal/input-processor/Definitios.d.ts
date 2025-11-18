import { InputProcessorConfigRNNoise, InputProcessorConfigWebRTC, InputProcessorStatistics } from "tc-shared/voice/RecorderBase";
export declare type ModalInputProcessorVariables = {
    propertyFilter: string;
} & InputProcessorConfigRNNoise & InputProcessorConfigWebRTC;
export interface ModalInputProcessorEvents {
    query_statistics: {};
    notify_statistics: {
        statistics: InputProcessorStatistics;
    };
    notify_apply_error: {
        message: string;
    };
}
