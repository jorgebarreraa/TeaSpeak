declare global {
    interface JQuery<TElement = HTMLElement> {
        asTabWidget(copy?: boolean): JQuery<TElement>;
        tabify(copy?: boolean): this;
        changeElementType(type: string): JQuery<TElement>;
    }
}
export declare const TabFunctions: {
    tabify(template: JQuery, copy?: boolean): JQuery;
};
