export interface SliderOptions {
    min_value?: number;
    max_value?: number;
    initial_value?: number;
    step?: number;
    unit?: string;
    value_field?: JQuery | JQuery[];
}
export interface Slider {
    value(value?: number): number;
}
export declare function sliderfy(slider: JQuery, options?: SliderOptions): Slider;
