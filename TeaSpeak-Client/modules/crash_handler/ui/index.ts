import { shell, ipcRenderer } from "electron";

function openIssueTracker() {
    shell.openExternal("https://github.com/TeaSpeak/TeaClient/issues");
}

function play_sound() {
    // Replaying sound from https://freesound.org/people/dersuperanton/sounds/435883/
    const node = document.createElement("audio");
    node.src = "./oh-oh.wav";
    node.volume = 1;
    document.body.append(node);
    node.play();
}
play_sound();

function set_dump_error_flag(flag: boolean) {
    for(const node of document.getElementsByClassName("error-show") as HTMLCollectionOf<HTMLElement>)
        node.style.display = flag ? "block" : "none";

    for(const node of document.getElementsByClassName("error-hide") as HTMLCollectionOf<HTMLElement>)
        node.style.display = flag ? "none" : "block";
}

function set_dump_url(url: string) {
    for(const crash_path_node of document.getElementsByClassName("crash-dump-directory") as HTMLCollectionOf<HTMLElement>) {
        crash_path_node.textContent = url;
        crash_path_node.onclick = () => shell.showItemInFolder(url);
    }
    set_dump_error_flag(false);
}

function set_dump_error(error: string) {
    set_dump_error_flag(true);
    for(const node of document.getElementsByClassName("crash-dump-error") as HTMLCollectionOf<HTMLElement>)
        node.textContent = error;
}

ipcRenderer.on('dump-url', (event, url) => set_dump_url(url));
ipcRenderer.on('dump-error', (event, error) => set_dump_error(error));