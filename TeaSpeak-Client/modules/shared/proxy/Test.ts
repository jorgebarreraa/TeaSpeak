import {ProxiedClass} from "./Definitions";
import {ObjectProxyClient} from "./Client";
import {ObjectProxyServer} from "./Server";

interface TextModal {
    readonly events: {
        onHide: () => void;
    }

    sayHi() : Promise<void>;
}

class TextModalImpl extends ProxiedClass<TextModal> implements TextModal {
    async sayHi(): Promise<void> {
        this.events.onHide();
    }
}

async function main() {
    let server = new ObjectProxyServer<TextModal>("", TextModalImpl);
    let client = new ObjectProxyClient<TextModal>("");

    const instance = await client.createNewInstance();
    await instance.sayHi();
    instance.events.onHide = () => {};
}