/// <reference types="react" />
import { Registry } from "tc-shared/events";
import { MicrophoneSettingsEvents } from "tc-shared/ui/modal/settings/MicrophoneDefinitions";
export declare const MicrophoneSettings: (props: {
    events: Registry<MicrophoneSettingsEvents>;
}) => JSX.Element;
