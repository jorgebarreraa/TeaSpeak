import { ClientServices } from "./ClientService";
import { ActionResult } from "./Action";
import { InviteAction } from "./Messages";
export declare type InviteLinkInfo = {
    linkId: string;
    timestampCreated: number;
    timestampDeleted: number;
    timestampExpired: number;
    amountViewed: number;
    amountClicked: number;
    propertiesConnect: {
        [key: string]: string;
    };
    propertiesInfo: {
        [key: string]: string;
    };
};
export declare class ClientServiceInvite {
    private readonly handle;
    constructor(handle: ClientServices);
    createInviteLink(connectProperties: {
        [key: string]: string;
    }, infoProperties: {
        [key: string]: string;
    }, createNew: boolean, expire_timestamp: number): Promise<ActionResult<{
        linkId: string;
        adminToken: string;
    }>>;
    queryInviteLink(linkId: string, registerView: boolean): Promise<ActionResult<InviteLinkInfo>>;
    logAction<A extends Exclude<InviteAction, InviteAction & {
        payload: any;
    }>["type"]>(linkId: string, action: A): Promise<ActionResult<void>>;
    logAction<A extends Extract<InviteAction, InviteAction & {
        payload: any;
    }>["type"]>(linkId: string, action: A, value: Extract<InviteAction, {
        payload: any;
        type: A;
    }>["payload"]): Promise<ActionResult<void>>;
}
