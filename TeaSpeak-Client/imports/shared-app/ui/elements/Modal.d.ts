export declare enum ElementType {
    HEADER = 0,
    BODY = 1,
    FOOTER = 2
}
export declare type BodyCreator = (() => JQuery | JQuery[] | string) | string | JQuery | JQuery[];
export declare const ModalFunctions: {
    divify: (val: JQuery) => JQuery<HTMLElement>;
    jqueriefy: (val: BodyCreator, type?: ElementType) => JQuery[] | JQuery | undefined;
    warpProperties(data: ModalProperties | any): ModalProperties;
};
export declare class ModalProperties {
    template?: string;
    header: BodyCreator;
    body: BodyCreator;
    footer: BodyCreator;
    closeListener: (() => void) | (() => void)[];
    registerCloseListener(listener: () => void): this;
    width: number | string;
    min_width?: number | string;
    height: number | string;
    closeable: boolean;
    triggerClose(): void;
    template_properties?: any;
    trigger_tab: boolean;
    full_size?: boolean;
}
export declare class Modal {
    private _htmlTag;
    properties: ModalProperties;
    shown: boolean;
    open_listener: (() => any)[];
    close_listener: (() => any)[];
    close_elements: JQuery;
    constructor(props: ModalProperties);
    get htmlTag(): JQuery;
    private _create;
    open(): void;
    close(): void;
    set_closeable(flag: boolean): void;
}
export declare function createModal(data: ModalProperties | any): Modal;
export declare class InputModalProperties extends ModalProperties {
    maxLength?: number;
    defaultValue?: string;
    field_title?: string;
    field_label?: string;
    field_placeholder?: string;
    error_message?: string;
}
export declare function createInputModal(headMessage: BodyCreator, question: BodyCreator, validator: (input: string) => boolean, callback: (flag: boolean | string) => void, props?: InputModalProperties | any): Modal;
export declare function createErrorModal(header: BodyCreator, message: BodyCreator, props?: ModalProperties | any): Modal;
export declare function createInfoModal(header: BodyCreator, message: BodyCreator, props?: ModalProperties | any): Modal;
