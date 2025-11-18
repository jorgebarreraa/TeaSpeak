import * as React from "react";
interface ErrorBoundaryState {
    errorOccurred: boolean;
}
export declare class ErrorBoundary extends React.PureComponent<{}, ErrorBoundaryState> {
    constructor(props: any);
    render(): React.ReactNode;
    componentDidCatch(error: Error, errorInfo: React.ErrorInfo): void;
    static getDerivedStateFromError(): Partial<ErrorBoundaryState>;
}
export {};
