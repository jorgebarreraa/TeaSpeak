export declare type Handle = {
    show(): any;
    is_shown(): any;
    hide(): any;
    update(): any;
};
export declare function initialize(entry: JQuery, callbacks?: {
    on_show?(tag: JQuery): any;
    on_hide?(tag: JQuery): any;
}): Handle;
