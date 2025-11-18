import * as electron from "electron";
import * as path from "path";
import * as fs from "fs-extra";
import {Updater} from "tc-shared/update/Updater";
import {ChangeLog, ChangeSetEntry} from "tc-shared/update/ChangeLog";
import {settings, Settings} from "tc-shared/settings";

function getChangeLogFile() {
    const app_path = electron.remote.app.getAppPath();
    if(app_path.endsWith(".asar")) {
        return path.join(path.dirname(app_path), "..", "ChangeLog.txt");
    } else {
        return path.join(app_path, "github", "ChangeLog.txt"); /* We've the source :D */
    }
}

const EntryRegex = /^([0-9]+)\.([0-9]+)\.([0-9]+)(-b[0-9]+)?:$/m;
function parseChangeLogEntry(lines: string[], index: number) : { entries: ChangeSetEntry[], index: number } {
    const entryDepth = lines[index].indexOf("-");
    if(entryDepth === -1) {
        throw "missing entry depth for line " + index;
    }

    let entries = [] as ChangeSetEntry[];
    let currentEntry;
    while(index < lines.length && !lines[index].match(EntryRegex)) {
        let trimmed = lines[index].trim();
        if(trimmed.length === 0) {
            index++;
            continue;
        }

        if(trimmed[0] === '-') {
            const depth = lines[index].indexOf('-');
            if(depth > entryDepth) {
                if(typeof currentEntry === "undefined") {
                    throw "missing change child entries parent at line " + index;
                }

                const result = parseChangeLogEntry(lines, index);
                entries.push({
                    changes: result.entries,
                    title: currentEntry
                });
                index = result.index;
            } else if(depth < entryDepth) {
                /* we're done with our block */
                break;
            } else {
                /* new entry */
                if(typeof currentEntry === "string") {
                    entries.push(currentEntry);
                }

                currentEntry = trimmed.substr(1).trim();
            }
        } else {
            currentEntry += "\n" + trimmed;
        }

        index++;
    }

    if(typeof currentEntry === "string") {
        entries.push(currentEntry);
    }

    return {
        index: index,
        entries: entries
    };
}

async function parseClientChangeLog() : Promise<ChangeLog> {
    let result: ChangeLog = {
        currentVersion: "unknown",
        changes: []
    }

    const lines = (await fs.readFile(getChangeLogFile())).toString("UTF-8").split("\n");
    let index = 0;

    while(index < lines.length && !lines[index].match(EntryRegex)) {
        index++;
    }

    while(index < lines.length) {
        const [ _, major, minor, patch, build ] = lines[index].match(EntryRegex);

        const entry = parseChangeLogEntry(lines, index + 1);
        result.changes.push({
            timestamp: major + "." + minor + "." + patch + (build || ""),
            changes: entry.entries
        });

        index = entry.index;
    }

    return result;
}

export class ClientUpdater implements Updater {
    private changeLog: ChangeLog;
    private currentVersion: string;

    constructor() {
    }

    async initialize() {
        this.currentVersion = electron.remote.getGlobal("app_version_client");
        this.changeLog = await parseClientChangeLog();
    }

    getChangeList(oldVersion: string): ChangeLog {
        let changes = {
            changes: [],
            currentVersion: this.currentVersion
        } as ChangeLog;

        for(const change of this.getChangeLog().changes) {
            if(change.timestamp === oldVersion)
                break;

            changes.changes.push(change);
        }

        return changes;
    }

    getChangeLog(): ChangeLog {
        return this.changeLog;
    }

    getCurrentVersion(): string {
        return this.currentVersion;
    }

    getLastUsedVersion(): string {
        const result = settings.getValue(Settings.KEY_UPDATER_LAST_USED_CLIENT, undefined);
        if(result === undefined) {
            /* We never have executed the client */
            this.updateUsedVersion();
            return this.getCurrentVersion();
        }

        return result;
    }

    updateUsedVersion() {
        settings.setValue(Settings.KEY_UPDATER_LAST_USED_CLIENT, this.getCurrentVersion());
    }
}