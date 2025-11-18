import { ChatEvent } from "../ui/frames/side/AbstractConversationDefinitions";
export declare function queryConversationEvents(clientUniqueId: string, query: {
    begin: number;
    end: number;
    direction: "backwards" | "forwards";
    limit: number;
}): Promise<{
    events: (ChatEvent & {
        databaseId: number;
    })[];
    hasMore: boolean;
}>;
export declare function registerConversationEvent(clientUniqueId: string, event: ChatEvent): Promise<void>;
