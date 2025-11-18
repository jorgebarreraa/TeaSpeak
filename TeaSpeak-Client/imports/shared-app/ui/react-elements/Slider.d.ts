import * as React from "react";
import { ReactElement } from "react";
import { Tooltip } from "tc-shared/ui/react-elements/Tooltip";
export interface SliderProperties {
    minValue: number;
    maxValue: number;
    stepSize: number;
    value: number;
    disabled?: boolean;
    className?: string;
    classNameFiller?: string;
    inverseFiller?: boolean;
    unit?: string;
    tooltip?: (value: number) => ReactElement | string | null;
    onInput?: (value: number) => void;
    onChange?: (value: number) => void;
    children?: never;
}
export interface SliderState {
    value: number;
    active: boolean;
    disabled?: boolean;
}
export declare class Slider extends React.Component<SliderProperties, SliderState> {
    private documentListenersRegistered;
    private lastValue;
    private readonly mouseListener;
    private readonly mouseUpListener;
    protected readonly refTooltip: React.RefObject<Tooltip>;
    protected readonly refSlider: React.RefObject<HTMLDivElement>;
    constructor(props: any);
    private unregisterDocumentListener;
    private registerDocumentListener;
    componentWillUnmount(): void;
    render(): JSX.Element;
    protected enableSliderMode(event: React.MouseEvent | React.TouchEvent): void;
    protected renderTooltip(): JSX.Element;
    componentDidUpdate(prevProps: Readonly<SliderProperties>, prevState: Readonly<SliderState>, snapshot?: any): void;
}
