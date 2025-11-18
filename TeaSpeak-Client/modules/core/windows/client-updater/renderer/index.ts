import {
    ipcRenderer
} from "electron";
import moment = require("moment");

const buttonCancel = document.getElementById("button-cancel");
const buttonSubmit = document.getElementById("button-submit");

const containerUpdateInfo = document.getElementById("container-info");
const containerUpdateExecute = document.getElementById("container-execute");

const updateStatusContainer = document.getElementById("update-availability-status");
const updateChannelSelect = document.getElementById("update-channel") as HTMLSelectElement;

const updateExecuteLog = document.getElementById("update-execute-log");
const updateExecuteProgress = document.getElementById("update-execute-progress");

let dotIndex = 0;
setInterval(() => {
    dotIndex++;
    let dots = ".";
    for(let index = 0; index < dotIndex % 3; index++) { dots += "."; }

    for(const dotContainer of document.getElementsByClassName("loading-dots")) {
        dotContainer.innerHTML = dots;
    }
}, 500);

const resetUpdateChannelDropdown = () => {
    while(updateChannelSelect.options.length > 0) {
        updateChannelSelect.options.remove(0);
    }

    for(const defaultOption of [{ text: "", value: "loading"}, {text: "???", value: "unknown" }]) {
        const element = document.createElement("option");
        element.text = defaultOption.text;
        element.value = defaultOption.value;
        element.style.display = "none";
        updateChannelSelect.options.add(element);
    }

    updateChannelSelect.onchange = undefined;
    updateChannelSelect.value = "loading";
}

ipcRenderer.on("client-updater-channel-info", (_event, available: string[], current: string) => {
    resetUpdateChannelDropdown();

    if(available.indexOf(current) === -1) {
        available.push(current);
    }

    for(const channel of available) {
        const element = document.createElement("option");
        element.text = channel;
        element.value = channel;
        updateChannelSelect.options.add(element);
    }

    updateChannelSelect.value = current;
    updateChannelSelect.onchange = () => {
        const value = updateChannelSelect.value;
        if(value === "loading" || value === "unknown") {
            return;
        }

        console.error("Update channel changed to %o", value);
        ipcRenderer.send("client-updater-set-channel", value);
        initializeVersionsView(false);
    }
});

ipcRenderer.on("client-updater-local-status", (_event, localVersion: string, buildTimestamp: number) => {
    document.getElementById("local-client-version").innerHTML = localVersion;
    document.getElementById("local-build-timestamp").innerHTML = moment(buildTimestamp).format("LTS, LL");
});

ipcRenderer.on("client-updater-set-error", (_event, message) => {
    for(const child of updateStatusContainer.querySelectorAll(".shown")) {
        child.classList.remove("shown");
    }

    const unavailableContainer = updateStatusContainer.querySelector(".unavailable");
    if(unavailableContainer) {
        unavailableContainer.classList.add("shown");

        const h2 = unavailableContainer.querySelector("h2");
        const h3 = unavailableContainer.querySelector("h3");

        if(h2) {
            h2.innerHTML = "Update failed!";
        }
        if(h3) {
            h3.innerHTML = message;
        }
    }

    /* TODO: Find out the current view and set the error */

    buttonSubmit.style.display = "none";
    buttonCancel.innerHTML = "Close";
});

const resetRemoteInfo = () => {
    document.getElementById("remote-client-version").innerText = "";
    document.getElementById("remote-build-timestamp").innerText = "";
}

ipcRenderer.on("client-updater-remote-status", (_event, updateAvailable: boolean, version: string, timestamp: number) => {
    resetRemoteInfo();

    for(const child of updateStatusContainer.querySelectorAll(".shown")) {
        child.classList.remove("shown");
    }

    updateStatusContainer.querySelector(updateAvailable ? ".available" : ".up2date")?.classList.add("shown");

    document.getElementById("remote-client-version").innerText = version;
    document.getElementById("remote-build-timestamp").innerText = moment(timestamp).format("LTS, LL");

    if(updateAvailable) {
        const h3 = updateStatusContainer.querySelector(".available h3");
        if(h3) {
            h3.innerHTML = "Update your client to " + version + ".";
        }
        buttonSubmit.innerHTML = "Update Client";
        buttonSubmit.style.display = null;
    }
});

function currentLogDate() : string {
    const now = new Date();
    return "<" + ("00" + now.getHours()).substr(-2) + ":" + ("00" + now.getMinutes()).substr(-2) + ":" + ("00" + now.getSeconds()).substr(-2) + "> ";
}

let followBottom = true;
let followBottomAnimationFrame;
const logUpdateExecuteInfo = (type: "info" | "error", message: string, extraClasses?: string[]) => {
    const element = document.createElement("div");

    if(message.length === 0) {
        element.innerHTML = "&nbsp;";
    } else {
        element.textContent = (!extraClasses?.length ? currentLogDate() + " " : "") + message;
    }
    element.classList.add("message", type, ...(extraClasses ||[]));
    updateExecuteLog.appendChild(element);

    if(!followBottomAnimationFrame && followBottom) {
        followBottomAnimationFrame = requestAnimationFrame(() => {
            followBottomAnimationFrame = undefined;

            if(!followBottom) { return; }
            updateExecuteLog.scrollTop = updateExecuteLog.scrollHeight;
        });
    }
}

updateExecuteLog.onscroll = () => {
    const bottomOffset = updateExecuteLog.scrollTop + updateExecuteLog.clientHeight;
    followBottom = bottomOffset + 50 > updateExecuteLog.scrollHeight;
};


ipcRenderer.on("client-updater-execute", () => initializeExecuteView());

ipcRenderer.on("client-updater-execute-log", (_event, type: "info" | "error", message: string) => {
    message.split("\n").forEach(line => logUpdateExecuteInfo(type, line))
});

const setExecuteProgress = (status: "normal" | "error" | "success", message: string, progress: number) => {
    const barContainer = updateExecuteProgress.querySelector(".bar-container") as HTMLDivElement;
    if(barContainer) {
        [...barContainer.classList].filter(e => e.startsWith("type-")).forEach(klass => barContainer.classList.remove(klass));
        barContainer.classList.add("type-" + status);
    }
    const progressFiller = updateExecuteProgress.querySelector(".filler") as HTMLDivElement;
    if(progressFiller) {
        progressFiller.style.width = (progress * 100) + "%";
    }

    const progressText = updateExecuteProgress.querySelector(".text") as HTMLDivElement;
    if(progressText) {
        progressText.textContent = (progress * 100).toFixed() + "%";
    }

    const progressInfo = updateExecuteProgress.querySelector(".info") as HTMLDivElement;
    if(progressInfo) {
        progressInfo.textContent = message;
    }
}

ipcRenderer.on("client-updater-execute-progress", (_event, message: string, progress: number) => setExecuteProgress("normal", message, progress));

ipcRenderer.on("client-updater-execute-finish", (_event, error: string | undefined) => {
    logUpdateExecuteInfo("info", "");
    logUpdateExecuteInfo("info", "Update result", ["centered"]);
    logUpdateExecuteInfo("info", "");

    buttonCancel.style.display = null;
    if(error) {
        /* Update failed */
        logUpdateExecuteInfo("error", "Failed to execute update: " + error);
        setExecuteProgress("error", "Update failed", 1);

        buttonSubmit.textContent = "Retry";
        buttonSubmit.style.display = null;
        buttonSubmit.onclick = () => initializeVersionsView(true);

        buttonCancel.textContent = "Close";
    } else {
        setExecuteProgress("success", "Update loaded", 1);
        logUpdateExecuteInfo("info", "Update successfully loaded.");
        logUpdateExecuteInfo("info", "Click \"Install Update\" to update your client.");
        buttonSubmit.textContent = "Install Update";
        buttonSubmit.style.display = null;
        buttonSubmit.onclick = () => ipcRenderer.send("install-update");

        buttonCancel.textContent = "Abort Update";
    }
});

buttonCancel.onclick = () => {
    ipcRenderer.send("client-updater-close");
};

const initializeExecuteView = () => {
    while(updateExecuteLog.firstChild) {
        updateExecuteLog.removeChild(updateExecuteLog.firstChild);
    }

    {
        const filler = document.createElement("div");
        filler.classList.add("filler");
        updateExecuteLog.appendChild(filler);
    }

    setExecuteProgress("normal", "Loading client update", 0);

    containerUpdateExecute.classList.add("shown");
    containerUpdateInfo.classList.remove("shown");

    buttonCancel.style.display = "none";
    buttonSubmit.onclick = undefined;
}

const initializeVersionsView = (queryLocalInfo: boolean) => {
    containerUpdateExecute.classList.remove("shown");
    containerUpdateInfo.classList.add("shown");

    for(const child of updateStatusContainer.querySelectorAll(".shown")) {
        child.classList.remove("shown");
    }
    updateStatusContainer.querySelector(".loading")?.classList.add("shown");
    resetUpdateChannelDropdown();
    resetRemoteInfo();

    if(queryLocalInfo) {
        ipcRenderer.send("client-updater-query-local-info");
    }

    ipcRenderer.send("client-updater-query-channels");
    ipcRenderer.send("client-updater-query-remote-info");
    buttonSubmit.onclick = () => ipcRenderer.send("execute-update");
    buttonSubmit.style.display = "none";
    buttonCancel.innerHTML = "Close";
}

initializeVersionsView(true);

export = {};