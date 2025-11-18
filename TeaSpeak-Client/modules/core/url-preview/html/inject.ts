declare let __teaclient_preview_notice: () => any;
declare let __teaclient_preview_error;

const electron = require("electron");
const log_prefix = "[TeaSpeak::Preview] ";

const html_overlay =
"<div style='position: fixed; top: 0; bottom: 0; left: 0; right: 0; z-index: 99999999999999999999999999;'>" +
    "<div style='\n" +
                "font-family: \"Open Sans\"," +
                "sans-serif;\n" +
                "width: 100%;\n" +
                "margin: 0;\n" +
                "height: 40px;\n" +
                "font-size: 17px;\n" +
                "font-weight: 400;\n" +
                "padding: .33em .5em;\n" +
                "color: #5c5e60;\n" +
                "position: fixed;\n" +
                "background-color: white;\n" +
                "box-shadow: 0 1px 3px 2px rgba(0,0,0,0.15);" +
                "display: flex;\n" +
                "flex-direction: row;\n" +
                "justify-content: center;" +
                "align-items: center;'" +
    ">" +
        "<div style='margin-right: .67em;display: inline-block;line-height: 1.3;text-align: center'>You're in TeaWeb website preview mode. Click <a href='#' class='button-open'>here</a> to open the website in the browser</div>" +
    "</div>" +
    "<div style='display: table-cell;width: 1.6em;'>" +
        "<a style='font-size: 14px;\n" +
                    "top: 13px;\n" +
                    "right: 25px;\n" +
                    "width: 15px;\n" +
                    "height: 15px;\n" +
                    "opacity: .3;\n" +
                    "color: #000;\n" +
                    "cursor: pointer;\n" +
                    "position: absolute;\n" +
                    "text-align: center;\n" +
                    "line-height: 15px;\n" +
                    "z-index: 1000;\n" +
                    "text-decoration: none;' " +
        "class='button-close'>" +
            "✖" +
        "</a>" +
    "</div>" +
"</div>";

let _close_overlay: () => void;
let _inject_overlay = () => {
    const element = document.createElement("div");
    element.id = "TeaClient-Overlay-Container";
    document.body.append(element);
    element.innerHTML = html_overlay;

    {
        _close_overlay = () => {
            console.trace(log_prefix + "Closing preview notice");
            element.remove();
        };

        const buttons = element.getElementsByClassName("button-close");
        if(buttons.length < 1) {
            console.warn(log_prefix + "Failed to find close button for preview notice!");
        } else {
            for(const button of buttons) {
                (button as HTMLElement).onclick = _close_overlay;
            }
        }
    }
    {
        const buttons = element.getElementsByClassName("button-open");
        if(buttons.length < 1) {
            console.warn(log_prefix + "Failed to find open button for preview notice!");
        } else {
            for(const element of buttons) {
                (element as HTMLElement).onclick = () => {
                    console.info(log_prefix + "Opening URL with default browser");
                    electron.remote.shell.openExternal(location.href, {
                        activate: true
                    }).catch(error => {
                        console.warn(log_prefix + "Failed to open URL in browser window: %o", error);
                    }).then(() => {
                    window.close();
                    });
                };
            }
        }
    }
};

/* Put this into the global scope. But we dont leek some nodejs stuff! */
console.log(log_prefix + "Script loaded waiting to be called!");
__teaclient_preview_notice = () => {
    if(_inject_overlay) {
        console.log(log_prefix + "TeaClient overlay called. Showing overlay.");
        _inject_overlay();
    } else {
        console.warn(log_prefix + "TeaClient overlay called, but overlay method undefined. May an load error occured?");
    }
};

const html_error = (error_code, error_desc, url) =>
"<div style='background-color: whitesmoke; padding: 40px; margin: 20px; font-family: consolas,serif;'>" +
    "<h2 align=center>Oops, this page failed to load correctly.</h2>" +
    "<p align=center><i>ERROR [ " + error_code + ", " + error_desc + " ]</i></p>" +
    '<br/><hr/>' +
    '<h4>Try this</h4>' +
    '<li type=circle>Check your spelling - <b>"' + url + '".</b></li><br/>' +
    '<li type=circle><a href="javascript:location.reload();">Refresh</a> the page.</li><br/>' +
    '<li type=circle>Perform a <a href=javascript:location.href="https://www.google.com/search?q=' + url + '">search</a> instead.</li><br/>' +
"</div>";

__teaclient_preview_error = (error_code, error_desc, url) => {
    document.body.innerHTML = html_error(decodeURIComponent(error_code), decodeURIComponent(error_desc), decodeURIComponent(url));
    _inject_overlay = undefined;
    if(_close_overlay) _close_overlay();
};