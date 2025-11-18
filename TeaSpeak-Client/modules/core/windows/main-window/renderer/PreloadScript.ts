/* preloaded script, init hook will be called before the loader will be executed */
declare global {
    interface Window {
        __native_client_init_hook: () => void;
        __native_client_init_shared: (webpackRequire: any) => void;
    }
}

window.__native_client_init_hook = () => require("../../../../renderer/index");
window.__native_client_init_shared = webpackRequire => window["shared-require"] = webpackRequire;

export = {};