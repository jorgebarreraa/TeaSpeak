import { Element } from "../elements";
import { StringRenderer } from "./base";
export default class extends StringRenderer {
    protected doRender(element: Element): (Element | string)[] | string;
}
