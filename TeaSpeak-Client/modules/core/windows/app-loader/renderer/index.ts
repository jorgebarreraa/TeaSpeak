import { ipcRenderer } from "electron";

const currentStatus = document.getElementById("current-status") as HTMLDivElement;
const progressIndicator = document.getElementById("progress-indicator") as HTMLDivElement;

const setStatusText = (text: string) => {
    if(currentStatus) {
        currentStatus.innerHTML = text;
    }
}

const setProgressIndicator = (value: number) => {
    if(progressIndicator) {
        progressIndicator.style.width = Math.min(value * 100, 100) + "%";
    }
}

ipcRenderer.on('progress-update', (event, status, count) => {
    console.log("Process update \"%s\" to %d", status, count);

    setStatusText(status);
    setProgressIndicator(count);
});

ipcRenderer.on('await-update', (event) => {
    console.log("Received update notification");

    setProgressIndicator(1);
    setStatusText("Awaiting client update response<br>(User input required)");
});

export = {};