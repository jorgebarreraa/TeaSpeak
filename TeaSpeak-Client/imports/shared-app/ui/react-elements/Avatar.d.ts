import * as React from "react";
import { ClientAvatar } from "tc-shared/file/Avatars";
export declare const AvatarRenderer: React.MemoExoticComponent<(props: {
    avatar: ClientAvatar | "loading" | "default";
    className?: string;
    alt?: string;
}) => JSX.Element>;
