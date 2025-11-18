import * as electron from "electron";
import {NativeImage} from "electron";
import * as loader from "tc-loader";
import {Stage} from "tc-loader";

import {
    ClientIcon,
    spriteEntries as kClientSpriteEntries,
    spriteHeight as kClientSpriteHeight,
    spriteUrl as kClientSpriteUrl,
    spriteWidth as kClientSpriteWidth
} from "svg-sprites/client-icons";
import {RemoteIcon} from "tc-shared/file/Icons";
import {LogCategory, logError} from "tc-shared/log";

let nativeSprite: NativeImage;

export function clientIconClassToImage(klass: string) : NativeImage {
    const sprite = kClientSpriteEntries.find(e => e.className === klass);
    if(!sprite) return undefined;

    return nativeSprite.crop({
        height: sprite.height,
        width: sprite.width,
        x: sprite.xOffset,
        y: sprite.yOffset
    });
}

export class RemoteIconDatafier {
    private cachedIcons: {[key: string]:{ refCount: number, icon: RemoteIconWrapper }} = {};
    private cleanupTimer;

    constructor() { }

    destroy() {
        clearTimeout(this.cleanupTimer);
    }

    resolveIcon(icon: RemoteIcon) : RemoteIconWrapper {
        const uniqueId = icon.iconId + "-" + icon.serverUniqueId;
        if(!this.cachedIcons[uniqueId]) {
            this.cachedIcons[uniqueId] = {
                refCount: 0,
                icon: new RemoteIconWrapper(uniqueId, icon)
            }
        }

        const cache = this.cachedIcons[uniqueId];
        cache.refCount++;
        return cache.icon;
    }

    unrefIcon(icon: RemoteIconWrapper) {
        const cache = this.cachedIcons[icon.uniqueId];
        if(!cache) { return; }

        cache.refCount--;
        if(cache.refCount <= 0) {
            if(this.cleanupTimer) {
                clearTimeout(this.cleanupTimer);
            }
            this.cleanupTimer = setTimeout(() => this.cleanupIcons(), 10 * 1000);
        }
    }

    private cleanupIcons() {
        this.cleanupTimer = undefined;
        for(const key of Object.keys(this.cachedIcons)) {
            if(this.cachedIcons[key].refCount <= 0) {
                this.cachedIcons[key].icon.destroy();
                delete this.cachedIcons[key];
            }
        }
    }
}
export const remoteIconDatafier = new RemoteIconDatafier();

export class RemoteIconWrapper {
    readonly callbackUpdated: ((newUrl: string) => void)[] = [];
    readonly uniqueId: string;

    private readonly icon: RemoteIcon;
    private readonly callbackStateChanged: () => void;
    private dataUrl: string | undefined;
    private currentImageUrl: string;

    constructor(uniqueId: string, icon: RemoteIcon) {
        this.icon = icon;
        this.uniqueId = uniqueId;
        this.callbackStateChanged = this.handleIconStateChanged.bind(this);

        this.icon.events.on("notify_state_changed", this.callbackStateChanged);
        this.handleIconStateChanged();
    }

    destroy() {
        this.icon.events.off("notify_state_changed", this.callbackStateChanged);
        this.currentImageUrl = undefined;
    }

    getDataUrl() : string | undefined { return this.dataUrl; }

    onDataUrlChange(callback: (newUrl: string) => void) : () => void {
        this.callbackUpdated.push(callback);
        return () => {
            const index = this.callbackUpdated.indexOf(callback);
            if(index !== -1) { this.callbackUpdated.splice(index, 1); }
        }
    }

    private async handleIconStateChanged() {
        if(this.icon.getState() === "loaded") {
            let imageUrl, dataUrl;
            try {
                if(this.icon.iconId >= 0 && this.icon.iconId <= 1000) {
                    imageUrl = "local-" + this.icon.iconId;
                    dataUrl = clientIconClassToImage(ClientIcon["Group_" + this.icon.iconId]).toDataURL();
                } else {
                    imageUrl = this.icon.getImageUrl();
                    this.currentImageUrl = imageUrl;

                    const image = new Image();
                    image.src = imageUrl;

                    await new Promise((resolve, reject) => {
                        image.onload = resolve;
                        image.onerror = reject;
                    });

                    if(this.currentImageUrl !== imageUrl) { return; }

                    const canvas = document.createElement("canvas");
                    if(image.naturalWidth > 1000 || image.naturalHeight > 1000) {
                        throw "image dimensions are too large";
                    } else {
                        canvas.width = image.naturalWidth;
                        canvas.height = image.naturalHeight;
                    }

                    canvas.getContext("2d").drawImage(image, 0, 0);
                    dataUrl = canvas.toDataURL();

                    /* We need to reset the current image URL in order to fire a changed event */
                    this.currentImageUrl = undefined;
                }
            } catch (error) {
                logError(LogCategory.GENERAL, tr("Failed to render remote icon %s-%d: %o"), this.icon.serverUniqueId, this.icon.iconId, error);

                imageUrl = "--error--" + Date.now();
                dataUrl = clientIconClassToImage(ClientIcon.Error).toDataURL();
            }

            this.setDataUrl(imageUrl, dataUrl);
        } else if(this.icon.getState() === "error") {
            this.setDataUrl(undefined, clientIconClassToImage(ClientIcon.Error).toDataURL());
        } else {
            this.setDataUrl(undefined, undefined);
        }
    }

    private setDataUrl(sourceImageUrl: string | undefined, dataUrl: string) {
        if(sourceImageUrl && this.currentImageUrl === sourceImageUrl) { return; }
        this.currentImageUrl = undefined; /* no image is loading any more */

        if(this.dataUrl === dataUrl) { return; }

        this.dataUrl = dataUrl;
        this.callbackUpdated.forEach(callback => callback(dataUrl));
    }
}

loader.register_task(Stage.JAVASCRIPT_INITIALIZING, {
    priority: 100,
    name: "native icon sprite loader",
    function: async () => {
        const image = new Image();
        image.src = loader.config.baseUrl + kClientSpriteUrl;
        await new Promise((resolve, reject) => {
            image.onload = resolve;
            image.onerror = () => reject("failed to load client icon sprite");
        });

        const canvas = document.createElement("canvas");
        canvas.width = kClientSpriteWidth;
        canvas.height = kClientSpriteHeight;
        canvas.getContext("2d").drawImage(image, 0, 0);

        nativeSprite = electron.remote.nativeImage.createFromDataURL(canvas.toDataURL());
    }
})

export function finalize() {
    nativeSprite = undefined;
}