export interface ShippedFileInfo {
    channel: string,
    version: string,
    git_hash: string,
    required_client: string,
    timestamp: number,
    filename: string
}

export default ShippedFileInfo;