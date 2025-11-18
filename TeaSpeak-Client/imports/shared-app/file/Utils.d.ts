export declare const downloadTextAsFile: (text: string, name: string) => void;
export declare const requestFile: (options: {
    accept?: string;
    multiple?: boolean;
}) => Promise<File[]>;
export declare const requestFileAsText: () => Promise<string>;
