import { Element, TagElement, TextElement } from "../elements";
export declare abstract class Renderer<T> {
    private textRenderer;
    private knownRenderer;
    render(element: Element, skipCustomRenderers?: boolean): T;
    renderContent(element: Element, skipCustomRenderers?: boolean): T[];
    protected abstract renderDefault(element: Element): T;
    getTextRenderer(): ElementRenderer<TextElement, T> | undefined;
    setTextRenderer(renderer: ElementRenderer<TextElement, T> | undefined): void;
    registerCustomRenderer(renderer: ElementRenderer<TagElement, T>): void;
    deleteCustomRenderer(tag: string): void;
    listCustomRenderers(): string[];
    getCustomRenderer(key: string): ElementRenderer<TagElement, T> | undefined;
}
export declare abstract class ElementRenderer<E extends Element, T, R extends Renderer<T> = Renderer<T>> {
    abstract tags(): string | string[];
    abstract render(element: E, renderer: R): T;
}
export declare abstract class StringRenderer extends Renderer<string> {
    protected renderDefault(element: Element): string;
    protected abstract doRender(element: Element): (Element | string)[] | string;
}
