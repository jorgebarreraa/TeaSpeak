export interface AppInfoFile {
    version: 2,

    clientVersion: {
        major: number,
        minor: number,
        patch: number,

        buildIndex: number,

        timestamp: number
    },

    /* The channel where the client has been downloaded from */
    clientChannel: string,

    /* The channel where UI - Packs should be downloaded from */
    uiPackChannel: string
}

export default AppInfoFile;