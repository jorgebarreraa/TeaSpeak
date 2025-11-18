import * as React from "react";
import { ChangeLog } from "tc-shared/update/ChangeLog";
import { Translatable } from "tc-shared/ui/react-elements/i18n";
export interface DisplayableChangeList extends ChangeLog {
    title: React.ReactElement<Translatable>;
    url: string;
}
export declare const WhatsNew: (props: {
    changesUI?: ChangeLog;
    changesClient?: ChangeLog;
}) => JSX.Element;
