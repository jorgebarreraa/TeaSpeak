import * as React from "react";
import "!style-loader!css-loader?url=false!sass-loader?sourceMap=true!react-resizable/css/styles.css";
import "!style-loader!css-loader?url=false!sass-loader?sourceMap=true!react-grid-layout/css/styles.css";
export declare type SpotlightDimensions = {
    width: number;
    height: number;
};
export declare const SpotlightDimensionsContext: React.Context<SpotlightDimensions>;
export declare const Spotlight: () => JSX.Element;
