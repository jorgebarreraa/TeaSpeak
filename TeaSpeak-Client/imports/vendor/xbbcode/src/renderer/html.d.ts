import { Element } from "../elements";
import { Renderer } from "./base";
import ReactRenderer from "./react";
export default class extends Renderer<string> {
    readonly reactRenderer: ReactRenderer | undefined;
    constructor(reactRenderer?: ReactRenderer);
    protected renderDefault(element: Element): string;
}
