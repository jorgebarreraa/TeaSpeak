/// <reference types="react" />
import { ReactComponentBase } from "tc-shared/ui/react-elements/ReactComponentBase";
import { ClientIcon } from "svg-sprites/client-icons";
export interface ButtonState {
    switched: boolean;
    dropdownShowed: boolean;
    dropdownForceShow: boolean;
}
export interface ButtonProperties {
    colorTheme?: "red" | "default";
    autoSwitch: boolean;
    tooltip?: string;
    iconNormal: string | ClientIcon;
    iconSwitched?: string | ClientIcon;
    onToggle?: (state: boolean) => boolean | void;
    className?: string;
    switched?: boolean;
}
export declare class Button extends ReactComponentBase<ButtonProperties, ButtonState> {
    protected defaultState(): ButtonState;
    render(): JSX.Element;
    private onMouseEnter;
    private onMouseLeave;
    private onClick;
}
