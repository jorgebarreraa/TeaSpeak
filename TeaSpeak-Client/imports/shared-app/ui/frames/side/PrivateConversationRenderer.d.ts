/// <reference types="react" />
import { Registry } from "tc-shared/events";
import { PrivateConversationUIEvents } from "tc-shared/ui/frames/side/PrivateConversationDefinitions";
export declare const PrivateConversationsPanel: (props: {
    events: Registry<PrivateConversationUIEvents>;
    handlerId: string;
}) => JSX.Element;
