import * as loader from "tc-loader";
import {Stage} from "tc-loader";
import {ScreenCaptureDevice, setVideoDriver, VideoSource} from "tc-shared/video/VideoSource";
import {WebVideoDriver, WebVideoSource} from "tc-shared/media/Video";
import {desktopCapturer, remote} from "electron";
import {requestMediaStreamWithConstraints} from "tc-shared/media/Stream";
import {tr} from "tc-shared/i18n/localize";

loader.register_task(Stage.JAVASCRIPT_INITIALIZING, {
    priority: 10,
    function: async () => {
        const instance = new NativeVideoDriver();
        await instance.initialize();
        setVideoDriver(instance);
    },
    name: "Video init"
});

class NativeVideoDriver extends WebVideoDriver {
    private currentScreenCaptureDevices: ScreenCaptureDevice[];

    screenQueryAvailable(): boolean {
        return true;
    }

    async queryScreenCaptureDevices(): Promise<ScreenCaptureDevice[]> {
        const sources = await desktopCapturer.getSources({ fetchWindowIcons: true, types: ['window', 'screen'], thumbnailSize: { width: 480, height: 270 } });

        return this.currentScreenCaptureDevices = sources.map(entry => {
            return {
                id: entry.id,
                name: entry.name,

                type: entry.display_id ? "full-screen" : "window",

                appIcon: entry.appIcon?.toDataURL(),
                appPreview: entry.thumbnail?.toDataURL()
            }
        })
    }

    async createScreenSource(id: string | undefined, allowFocusLoss: boolean): Promise<VideoSource> {
        const result = await requestMediaStreamWithConstraints({
            mandatory: {
                chromeMediaSource: 'desktop',
                chromeMediaSourceId: id,
            }
        } as any, "video");

        if(typeof result === "string") {
            throw result;
        }

        if(!allowFocusLoss) {
            /* redraw focus to our window since we lost it after requesting the screen capture */
            remote.getCurrentWindow().focus();
        }

        const name = this.currentScreenCaptureDevices.find(e => e.id === id)?.name || tr("Screen device");
        return new WebVideoSource(id, name, result);
    }
}