import * as React from "react";
import { Registry } from "tc-shared/events";
import '!style-loader!css-loader!emoji-mart/css/emoji-mart.css';
interface ChatBoxEvents {
    action_set_enabled: {
        enabled: boolean;
    };
    action_request_focus: {};
    action_submit_message: {
        message: string;
    };
    action_insert_text: {
        text: string;
        focus?: boolean;
    };
    notify_typing: {};
}
export interface ChatBoxProperties {
    className?: string;
    onSubmit?: (text: string) => void;
    onType?: () => void;
}
export interface ChatBoxState {
    enabled: boolean;
}
export declare class ChatBox extends React.Component<ChatBoxProperties, ChatBoxState> {
    readonly events: Registry<ChatBoxEvents>;
    private callbackSubmit;
    private callbackType;
    constructor(props: any);
    componentDidMount(): void;
    componentWillUnmount(): void;
    render(): JSX.Element;
    componentDidUpdate(prevProps: Readonly<ChatBoxProperties>, prevState: Readonly<ChatBoxState>, snapshot?: any): void;
}
export {};
