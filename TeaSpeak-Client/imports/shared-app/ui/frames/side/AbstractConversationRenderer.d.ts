import * as React from "react";
import { Registry } from "tc-shared/events";
import { AbstractConversationUiEvents } from "./AbstractConversationDefinitions";
export declare const ConversationPanel: React.MemoExoticComponent<(props: {
    events: Registry<AbstractConversationUiEvents>;
    handlerId: string;
    messagesDeletable: boolean;
    noFirstMessageOverlay: boolean;
}) => JSX.Element>;
