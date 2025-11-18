#include <algorithm>
#include <log/LogUtils.h>
#include <misc/memtracker.h>
#include <misc/sassert.h>
#include <misc/utf8.h>
#include "misc/rnd.h"
#include "src/VirtualServer.h"
#include "src/client/ConnectedClient.h"
#include "src/InstanceHandler.h"
#include "../manager/ConversationManager.h"
#include "../groups/GroupManager.h"

using namespace std;
using namespace ts;
using namespace ts::server;

ServerChannel::ServerChannel(uint32_t rtc_channel_id, ChannelId parentId, ChannelId channelId) : BasicChannel(parentId, channelId),
                                                                                                                                  rtc_channel_id{rtc_channel_id} {
    memtrack::allocated<ServerChannel>(this);
}

ServerChannel::~ServerChannel() {
    memtrack::freed<ServerChannel>(this);
}

void ServerChannel::register_client(const std::shared_ptr<ts::server::ConnectedClient> &client) {
    unique_lock lock(this->client_lock);
    for(const auto& _client : this->clients) {
        if(_client.lock() == client) {
            return;
        }
    }

    this->clients.push_back(client);
}

void ServerChannel::unregister_client(const std::shared_ptr<ts::server::ConnectedClient> &client) {
    unique_lock lock(this->client_lock);
    this->clients.erase(remove_if(this->clients.begin(), this->clients.end(), [client](const auto& weak) {
        auto locked = weak.lock();
        if(!locked || client == locked) return true;
        return false;
    }), this->clients.end());
}

size_t ServerChannel::client_count() {
    shared_lock lock(this->client_lock);
    size_t result = 0;
    for(const auto& weak_entry : this->clients) {
        if(auto entry = weak_entry.lock()) {
            auto current_channel = entry->getChannel();
            if(current_channel && current_channel.get() == this)
                result++;
        }
    }
    return result;
}

void ServerChannel::setProperties(const std::shared_ptr<PropertyManager> &ptr) {
    BasicChannel::setProperties(ptr);
}

ServerChannelTree::ServerChannelTree(const std::shared_ptr<server::VirtualServer>& server, sql::SqlManager* sql) : sql(sql), server_ref(server) { }

ServerChannelTree::~ServerChannelTree() { }

void ServerChannelTree::deleteSemiPermanentChannels() {
    loop:

    for(const auto& ch : this->channels()) {
        if(ch->channelType() == ChannelType::semipermanent || ch->channelType() == ChannelType::temporary){ //We also delete private channels
            this->delete_channel_root(ch);
            goto loop;
        }
    }
}

ChannelId ServerChannelTree::generateChannelId() {
    ChannelId channelId = 0;

    auto res = sql::command(this->sql, "SELECT `channelId` FROM `channels` WHERE `serverId` = :sid ORDER BY `channelId` DESC LIMIT 1", variable{":sid", this->getServerId()}).query([](ChannelId* num, int, char** value, char**){
        *num = (ChannelId) stoll(value[0]);
        return 0;
    }, &channelId);
    auto pf = LOG_SQL_CMD;
    pf(res);
    if(!res) return 0;
    return channelId + 1;
}

std::shared_ptr<BasicChannel> ServerChannelTree::createChannel(ChannelId parentId, ChannelId orderId, const string &name) {
    std::shared_ptr<BasicChannel> channel = BasicChannelTree::createChannel(parentId, orderId, name);
    if(!channel) return channel;

    /* TODO: Speed up (skip the database query) */
    auto properties = serverInstance->databaseHelper()->loadChannelProperties(this->server_ref.lock(), channel->channelId());
    for(const auto& prop : channel->properties()->list_properties()) {
        if(prop.isModified()) { //Copy the already set properties
            (*properties)[prop.type()] = prop.value();
        }
    }

    static_pointer_cast<ServerChannel>(channel)->setProperties(properties);
    static_pointer_cast<ServerChannel>(channel)->setPermissionManager(serverInstance->databaseHelper()->loadChannelPermissions(this->server_ref.lock(), channel->channelId()));

    channel->properties()[property::CHANNEL_CREATED_AT] = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
    channel->properties()[property::CHANNEL_LAST_LEFT] = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();


    auto result = sql::command(this->sql, "INSERT INTO `channels` (`serverId`, `channelId`, `parentId`) VALUES(:sid, :chid, :parent);",  variable{":sid", this->getServerId()}, variable{":chid", channel->channelId()}, variable{":parent", channel->parent() ? channel->parent()->channelId() : 0}).execute();
    auto pf = LOG_SQL_CMD;
    pf(result);

    return channel;
}

inline std::shared_ptr<BasicChannel> findChannelByPool(const std::deque<std::shared_ptr<TreeView::LinkedTreeEntry>>& pool, size_t chId){
    for(const auto& elm : pool)
        if(elm->entry->channelId() == chId) return dynamic_pointer_cast<BasicChannel>(elm->entry);
    return nullptr;
}

inline std::shared_ptr<TreeView::LinkedTreeEntry> findLinkedChannelByPool(const std::deque<std::shared_ptr<TreeView::LinkedTreeEntry>>& pool, size_t chId){
    for(const auto& elm : pool)
        if(elm->entry->channelId() == chId) return elm;
    return nullptr;
}

ServerId ServerChannelTree::getServerId() {
    auto s = this->server_ref.lock();
    return s ? s->getServerId() : 0UL;
}

bool ServerChannelTree::initializeTempParents() {
    auto channelList = this->tmpChannelList;
    for(const auto& linked_channel : channelList) {
        auto channel = dynamic_pointer_cast<BasicChannel>(linked_channel->entry);
        assert(channel);

        if(channel->properties()[property::CHANNEL_PID].as_or<ChannelId>(0) != 0){
            if(!channel->parent())
                linked_channel->parent = findLinkedChannelByPool(this->tmpChannelList, channel->properties()[property::CHANNEL_PID]);
            if(!channel->parent()){
                logError(this->getServerId(), "Invalid channel parent (Channel does not exists). Channel id: {} ({}) Missing parent id: {}", channel->channelId(), channel->name(),
                         channel->properties()[property::CHANNEL_PID].as_or<ChannelId>(0));
                logError(this->getServerId(), "Resetting parent");
                channel->properties()[property::CHANNEL_PID] = 0;
            }
        }
    }
    return true;
}

typedef std::deque<std::shared_ptr<TreeView::LinkedTreeEntry>> ChannelPool;
inline ChannelPool remoteTopChannels(const std::shared_ptr<TreeView::LinkedTreeEntry>& parent, ChannelPool& pool){
    ChannelPool result;
    for(const auto& elm : pool)
        if(elm->parent.lock() == parent)
            result.push_back(elm);
    for(const auto& elm : result)
        pool.erase(std::find(pool.begin(), pool.end(), elm));
    return result;
}

inline ChannelPool resolveChannelHeads(ServerId serverId, ChannelPool pool) {
    ChannelPool result;

    while(!pool.empty()) {
        auto element = pool.front();
        if(!element) continue;

        ChannelPool tmp_pool = pool;
        auto f = find(tmp_pool.begin(), tmp_pool.end(), element);
        if(f != tmp_pool.end())
            tmp_pool.erase(f);

        while(element->previous) {
            auto found = find(tmp_pool.begin(), tmp_pool.end(), element->previous);
            if(found == tmp_pool.end()) {
                logError(serverId, "Found recursive channel heads! Cutting");
                if(element->previous && element->previous->next == element)
                    element->previous->next = nullptr;
                element->previous = nullptr;
                break;
            }
            tmp_pool.erase(found);
            element = element->previous;
        }; //Find the head
        result.push_back(element);

        auto last = element;
        while(element) {
            auto it = find(pool.begin(), pool.end(), element);
            if(it == pool.end()) {
                logError(serverId, "Circular tail. Cutting previus.");
                if(last != element) {
                    if(element->previous == last)
                        element->previous = nullptr;

                    if(last->next == element)
                        last->next = nullptr;
                }
                break;
            } else
                pool.erase(it);

            last = element;
            element = element->next;
        }
    }

    return result;
}

inline std::shared_ptr<TreeView::LinkedTreeEntry> buildChannelTree(ServerId serverId, const std::shared_ptr<TreeView::LinkedTreeEntry>& parent, ChannelPool& channel_pool){
    auto top_channels = remoteTopChannels(parent, channel_pool);
    if(top_channels.empty()) return nullptr;

    bool brokenTree = false;
    for(const auto& linked_channel : top_channels){
        auto channel = dynamic_pointer_cast<BasicChannel>(linked_channel->entry);
        assert(channel);

        if(channel->channelOrder() != 0) {
            if(channel->channelOrder() == channel->channelId()) {
                brokenTree = true;
                logError(serverId, "Channel order refers to itself! (Resetting)");
                channel->properties()[property::CHANNEL_ORDER] = 0;
                continue;
            }

            auto previous_linked = findLinkedChannelByPool(top_channels, channel->channelOrder());
            if(!previous_linked) {
                brokenTree = true;
                logError(serverId, "Failed to resolve previous channel for channel {} ({}). Previous channel id: {} (Resetting order)", channel->channelId(), channel->name(), channel->channelOrder());
                channel->properties()[property::CHANNEL_ORDER] = 0;
                continue;
            }

            if(previous_linked->next) {
                if(previous_linked->next != linked_channel) {
                    brokenTree = true;
                    logError(serverId, "Previous channel {} ({}) of channel {} ({}) is already linked with another channel {} ({}).",
                             previous_linked->entry->channelId(), dynamic_pointer_cast<BasicChannel>(previous_linked->entry)->name(),
                             channel->channelId(), channel->name(),
                             previous_linked->next->entry->channelId(), dynamic_pointer_cast<BasicChannel>(previous_linked->next->entry)->name()
                    );
                    logError(serverId, "Inserting channel anyway!");
                    previous_linked->next->previous = linked_channel;
                    linked_channel->next = previous_linked->next;
                }
            }
            previous_linked->next = linked_channel;
            linked_channel->previous = previous_linked;
        }
    }

    for(const auto& elm : top_channels){
        if(elm->next) {
            if(elm->next->previous != elm) {
                brokenTree = true;
                logError(serverId, "Test 'elm->next->previous != elm' failed! Assigning elm->next->previous to elm!");
                elm->next->previous = elm;
            }
        }
        if(elm->previous) {
            if(elm->previous->next != elm) {
                brokenTree = true;
                logError(serverId, "Test 'elm->previous->next != elm' failed! Assigning elm->previous->next to elm!");
                elm->previous->next = elm;
            }
        }
    }



    /* Get the heads and merge them */
    auto heads = resolveChannelHeads(serverId, top_channels);
    if(heads.size() > 1){
        brokenTree = true;
        logError(serverId, "Got multiple channel heads! (Count {})", heads.size());
        logError(serverId, "Try to appending them", heads.size());

        debugMessage(serverId, "Got head:");
        //FIXME print head

        for(int index = 1; index < heads.size(); index++) {
            auto tail = heads[0];
            while(tail->next) tail = tail->next;

            tail->next = heads[index];
            tail->next->previous = tail;
        }
    }

    /* Testing tree */

    if(brokenTree) {
        auto new_heads = resolveChannelHeads(serverId, top_channels);
        if(new_heads.size() != 1) {
            logCritical(serverId, "Failed to merge channel heads (Got {} heads)! Unknown reason!", new_heads.size());
            logCritical(serverId, "Dropping a part!");
        }
    }

    brokenTree = true;
    if(brokenTree){
        auto entry = heads[0];
        while(entry) {
            auto channel = dynamic_pointer_cast<BasicChannel>(entry->entry);
            assert(channel);

            auto evaluated_order_id = entry->previous ? entry->previous->entry->channelId() : 0;
            if(evaluated_order_id != channel->previousChannelId()) {
                channel->setPreviousChannelId(evaluated_order_id);
                debugMessage(serverId, "Fixed order id for channel {} ({}). New previous channel {}", entry->entry->channelId(), channel->name(), channel->channelOrder());
            }

            auto evaluated_parent_id = channel->parent() ? channel->parent()->channelId() : 0;
            if(evaluated_parent_id != channel->properties()[property::CHANNEL_PID].as_or<ChannelId>(0)) {
                debugMessage(serverId, "Fixed parent id for channel {} ({}). New parent channel {}", entry->entry->channelId(), channel->name(), evaluated_parent_id);
                channel->properties()[property::CHANNEL_PID] = evaluated_parent_id;
            }
            entry = entry->next;
        }
    }

    { /* building sub trees */
        auto entry = heads[0];
        while(entry) {
            entry->child_head = buildChannelTree(serverId, entry, channel_pool);
            entry = entry->next;
        }
    }

    return heads[0];
}

bool ServerChannelTree::buildChannelTreeFromTemp() {
    if(this->tmpChannelList.empty()) return true;

    this->head = buildChannelTree(this->getServerId(), nullptr, this->tmpChannelList);
    assert(tmpChannelList.empty());
    return true;
}

inline void walk_tree(const std::shared_ptr<TreeView::LinkedTreeEntry>& parent, std::shared_ptr<TreeView::LinkedTreeEntry> head) {
    auto parent_id = parent ? parent->entry->channelId() : 0;
    std::shared_ptr<TreeView::LinkedTreeEntry> previous{nullptr};
    while(head) {
        head->entry->setParentChannelId(parent_id);
        if(head->previous != previous) {
            logCritical(0, "Detect broken channel tree!");
        } else if(previous) {
            head->entry->setPreviousChannelId(previous->entry->channelId());
        } else {
            head->entry->setPreviousChannelId(0);
        }

        if(head->child_head)
            walk_tree(head, head->child_head);
        previous = head;
        head = head->next;
    }
}

bool ServerChannelTree::updateOrderIds() {
    walk_tree(nullptr, this->head);
    return true;
}


inline ssize_t count_characters(const std::string& in) {
    size_t index = 0;
    size_t count = 0;
    while(index < in.length()){
        count++;

        auto current = (uint8_t) in[index];
        if(current >= 128) { //UTF8 check
            if(current >= 192 && (current <= 193 || current >= 245)) {
                return -1;
            } else if(current >= 194 && current <= 223) {
                if(in.length() - index <= 1) return -1;
                else if((uint8_t) in[index + 1] >= 128 && (uint8_t) in[index + 1] <= 191) index += 1; //Valid
                else return -1;
            } else if(current >= 224 && current <= 239) {
                if(in.length() - index <= 2) return -1;
                else if((uint8_t) in[index + 1] >= 128 && (uint8_t) in[index + 1] <= 191 &&
                        (uint8_t) in[index + 2] >= 128 && (uint8_t) in[index + 2] <= 191) index += 2; //Valid
                else return -1;
            } else if(current >= 240 && current <= 244) {
                if(in.length() - index <= 3) return -1;
                else if((uint8_t) in[index + 1] >= 128 && (uint8_t) in[index + 1] <= 191 &&
                        (uint8_t) in[index + 2] >= 128 && (uint8_t) in[index + 2] <= 191 &&
                        (uint8_t) in[index + 3] >= 128 && (uint8_t) in[index + 3] <= 191) index += 3; //Valid
                else return -1;
            } else {
                return -1;
            }
        }
        index++;
    }
    return count;
}

bool ServerChannelTree::validateChannelNames() {
    /*

    if (count_characters(cmd["channel_name"]) < 1) return {findError("channel_name_inuse"), "Invalid channel name (too short)"};
    if (count_characters(cmd["channel_name"]) > 40) return {findError("channel_name_inuse"), "Invalid channel name (too long)"};

     */
    /*
    for(const auto &channel : this->channels()){
        mainSearch:
        for(const auto& ref : this->channels(channel->parent(), 1))
            if(ref->name() == channel->name() && ref != channel) {
                logError(lstream << "Duplicated channel name '" << channel->name() << "'. Fixing it by appending '1'" << endl);
                channel->properties()["channel_name"] = channel->name() + "1";
                goto mainSearch;
            }
        if(channel->name().length() > 40) {
            channel->properties()["channel_name"] = channel->name().substr(0, 35) + rnd_string(5);
            goto mainSearch;
        }
    }
     */

    for(const auto &channel : this->channels()) {
        auto name_length = utf8::count_characters(channel->name());
        if(name_length > 40) {
            logError(this->getServerId(), "Channel {} loaded an invalid name from the database (name to long). Cutting channel name");
            channel->properties()[property::CHANNEL_NAME] = channel->name().substr(0, 40); //FIXME count UTF8
        } else if(name_length < 1) {
            logError(this->getServerId(), "Channel {} loaded an invalid name from the database (empty name). Resetting channel name");
            channel->properties()[property::CHANNEL_NAME] = "undefined";
        }
    }

    function<void(const std::shared_ptr<LinkedTreeEntry> &)> test_level;

    test_level = [&](const std::shared_ptr<LinkedTreeEntry> &head) {
        map<string, shared_ptr<ServerChannel>> used_names;
        auto it = head;
        while(it) {
            auto channel = dynamic_pointer_cast<ServerChannel>(it->entry);
            auto name = channel->name();

            if(used_names.count(name) > 0) {
                auto taken_channel = used_names[name];
                assert(taken_channel);

                size_t index = 1;
                while(true) {
                    auto _name = name + to_string(index);
                    if(_name.length() > 40) //FIXME count UTF8
                        _name = _name.substr(_name.length() - 40);
                    if(used_names[_name])
                        index++;
                    else {
                        name = _name;
                        break;
                    }
                }
                channel->properties()[property::CHANNEL_NAME] = name;

                logError(this->getServerId(), "Channel {} has the same name as channel {}. Name: {}. Changing name to {} by appending an index.",
                        channel->channelId(),
                        taken_channel->channelId(),
                        taken_channel->name(),
                        name
                );
            }
            used_names[name] = channel;

            if(it->child_head)
                test_level(it->child_head);
            it = it->next;
        }
    };

    test_level(this->tree_head());
    return true;
}

bool ServerChannelTree::validateChannelIcons() {
#if 0
    for(const auto &channel : this->channels()) {
        auto iconId = (IconId) channel->properties()[property::CHANNEL_ICON_ID];
        if(iconId != 0 && !serverInstance->getFileServer()->iconExists(this->server.lock(), iconId)) {
            logMessage(this->getServerId(), "[FILE] Missing channel icon (" + to_string(iconId) + ").");
            if(config::server::delete_missing_icon_permissions) {
                channel->properties()[property::CHANNEL_ICON_ID] = 0;
                channel->permissions()->set_permission(permission::i_icon_id, {0, 0}, permission::v2::PermissionUpdateType::set_value, permission::v2::PermissionUpdateType::do_nothing);
            }
        }
    }
#endif
    return true;
}

void ServerChannelTree::loadChannelsFromDatabase() {
    auto res = sql::command(this->sql, "SELECT  `channelId`, `parentId` FROM `channels` WHERE `serverId` = :sid", variable{":sid", this->getServerId()}).query(&ServerChannelTree::loadChannelFromData, this);
    (LOG_SQL_CMD)(res);
    if(!res){
        logError(this->getServerId(), "Could not load channel tree from database");
        return;
    }

    logMessage(this->getServerId(), "Loaded {} saved channels. Assembling...", this->tmpChannelList.size());
    this->initializeTempParents();
    this->buildChannelTreeFromTemp();
    this->updateOrderIds();
    this->validateChannelNames();
    this->validateChannelIcons();
    //this->printChannelTree();
}

int ServerChannelTree::loadChannelFromData(int argc, char **data, char **column) {
    ChannelId channelId = 0;
    ChannelId parentId = 0;


    int index = 0;
    try {
        for(index = 0; index < argc; index++){
            if(strcmp(column[index], "channelId") == 0) channelId = static_cast<ChannelId>(stoll(data[index]));
            else if(strcmp(column[index], "parentId") == 0) parentId = static_cast<ChannelId>(stoll(data[index]));
            else logError(this->getServerId(), "ServerChannelTree::loadChannelFromData called with invalid column from sql \"{}\"", column[index]);
        }
    } catch (std::exception& ex) {
        logError(this->getServerId(), "Failed to load channel. Got exception {}. Exception was thrown at parsing row {} with data \"{}\"", ex.what(), column[index], data[index]);
        return 0;
    }

    //assert(type != 0xFF);
    assert(channelId != 0);
    if(channelId == 0)
        return 0;

    auto server = this->server_ref.lock();

    std::shared_ptr<ServerChannel> channel;
    if(server) {
        auto rtc_channel_id = server->rtc_server().create_channel();

        channel = std::make_shared<ServerChannel>(rtc_channel_id, parentId, channelId);
    } else {
        channel = std::make_shared<ServerChannel>(0, parentId, channelId);
    }
    static_pointer_cast<ServerChannel>(channel)->setProperties(serverInstance->databaseHelper()->loadChannelProperties(server, channelId));
    static_pointer_cast<ServerChannel>(channel)->setPermissionManager(serverInstance->databaseHelper()->loadChannelPermissions(server, channel->channelId()));

    auto entry = make_shared<TreeView::LinkedTreeEntry>(channel);
    channel->setLinkedHandle(entry);
    this->tmpChannelList.push_back(entry);
    return 0;
}

deque<ChannelId> ServerChannelTree::deleteChannelRoot(const std::shared_ptr<BasicChannel> &channel) {
    auto server = this->server_ref.lock();

    auto channels = this->delete_channel_root(channel);
    deque<ChannelId> channel_ids;
    for(const auto& channel : channels) {
        channel_ids.push_back(channel->channelId());
    }
    return channel_ids;
}

void ServerChannelTree::on_channel_entry_deleted(const shared_ptr<BasicChannel> &channel) {
    BasicChannelTree::on_channel_entry_deleted(channel);

    auto server_channel = dynamic_pointer_cast<ServerChannel>(channel);
    assert(server_channel);

    auto server = this->server_ref.lock();
    if(server) {
        server->group_manager()->assignments().handle_channel_deleted(channel->channelId());
        server->conversation_manager()->delete_conversation(channel->channelId());
        server->rtc_server().destroy_channel(server_channel->rtc_channel_id);
    } else {
        serverInstance->group_manager()->assignments().handle_channel_deleted(channel->channelId());
    }


    auto sql_result = sql::command(this->sql, "DELETE FROM `channels` WHERE `serverId` = '" + to_string(this->getServerId()) + "' AND `channelId` = '" + to_string(channel->channelId()) + "'").execute();
    LOG_SQL_CMD(sql_result);

    sql_result = sql::command(this->sql, "DELETE FROM `properties` WHERE `serverId` = '" + to_string(this->getServerId()) + "' AND `id` = '" + to_string(channel->channelId()) + "' AND `type` = " + to_string(property::PropertyType::PROP_TYPE_CHANNEL)).execute();
    LOG_SQL_CMD(sql_result);

    serverInstance->databaseHelper()->deleteChannelPermissions(this->server_ref.lock(), channel->channelId());
    sql_result = sql::command(this->sql, "DELETE FROM `assignedGroups` WHERE `serverId` = '" + to_string(this->getServerId()) + "' AND `channelId` = '" + to_string(channel->channelId()) + "'").execute();
    LOG_SQL_CMD(sql_result);
}

std::shared_ptr<BasicChannel> ServerChannelTree::allocateChannel(const shared_ptr<BasicChannel> &parent, ChannelId channelId) {
    auto server = this->server_ref.lock();
    auto parent_channel_id = parent ? parent->channelId() : 0;
    if(server) {
        auto rtc_channel_id = server->rtc_server().create_channel();

        return std::make_shared<ServerChannel>(rtc_channel_id, parent_channel_id, channelId);
    } else {
        return std::make_shared<ServerChannel>(0, parent_channel_id, channelId);
    }
}
