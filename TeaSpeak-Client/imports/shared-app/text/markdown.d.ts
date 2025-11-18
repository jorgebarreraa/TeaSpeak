import { Env, Options, Token } from "remarkable/lib";
export declare class MD2BBCodeRenderer {
    private static renderers;
    private _options;
    currentLineCount: number;
    reset(): void;
    render(tokens: Token[], options: Options, env: Env): string;
    private renderToken;
    options(): any;
}
export declare function renderMarkdownAsBBCode(message: string, textProcessor: (text: string) => string): string;
