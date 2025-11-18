import * as React from "react";
import { Renderer } from "./base";
import { Element, TagElement } from "../elements";
export default class ReactRenderer extends Renderer<React.ReactNode> {
    private readonly encapsulateText;
    constructor(encapsulateText?: boolean);
    protected renderDefault(element: Element): React.ReactNode;
    private doRender;
    private renderTag;
    private renderText;
    renderAsText(element: Element | string, stripLeadingAnTailingEmptyLines: boolean): React.ReactNode;
    renderContentAsText(element: TagElement, stripLeadingAnTailingEmptyLines: boolean): React.ReactNode;
}
