#include "log/LogUtils.h"
#include <algorithm>
#include <iostream>
#include <mutex>
#include <sstream>
#include <misc/base64.h>
#include "query/Command.h"
#include "misc/digest.h"
#include "BasicChannel.h"
#include "query/escape.h"

using namespace std;
using namespace std::chrono;
using namespace ts;

BasicChannel::BasicChannel(ChannelId parentId, ChannelId channelId) {
    this->setProperties(make_shared<Properties>());

    this->_properties->register_property_type<property::ChannelProperties>();
    this->properties()[property::CHANNEL_ID] = channelId;
    this->properties()[property::CHANNEL_PID] = parentId;
}

void BasicChannel::setPermissionManager(const std::shared_ptr<permission::v2::PermissionManager>& manager) {
    this->_permissions = manager;
    this->update_properties_from_permissions();
}

void BasicChannel::setProperties(const std::shared_ptr<ts::Properties>& props) {
    if(this->_properties) {
        (*props)[property::CHANNEL_ID] = this->channelId();
        (*props)[property::CHANNEL_PID] = this->properties()[property::CHANNEL_PID].value();
    }
    this->_properties = props;

    this->properties().registerNotifyHandler([&](Property& prop){
        if(prop.type() == property::CHANNEL_FLAG_DEFAULT)
            this->properties()[property::CHANNEL_FLAG_PASSWORD] = false;
        else if(prop.type() == property::CHANNEL_ID)
            this->_channel_id = prop;
        else if(prop.type() == property::CHANNEL_ORDER)
            this->_channel_order = prop;
    });

    //Update cached variables
    if(props->has(property::CHANNEL_ORDER)) this->_channel_order = this->properties()[property::CHANNEL_ORDER];
    else this->_channel_order = 0;
    this->_channel_id = this->channelId();
}

std::vector<property::ChannelProperties> BasicChannel::update_properties_from_permissions() {
    std::vector<property::ChannelProperties> result;
    result.reserve(2);

    auto permission_manager = this->permissions(); /* keeps the manager until we've finished our calculations */
    /* update the icon id */
    {
        IconId target_icon_id = 0;
        auto& permission_icon_flags = permission_manager->permission_flags(permission::i_icon_id);
        if(permission_icon_flags.value_set)
            target_icon_id = (IconId) permission_manager->permission_values(permission::i_icon_id).value;
        if(this->properties()[property::CHANNEL_ICON_ID] != target_icon_id) {
            this->properties()[property::CHANNEL_ICON_ID] = target_icon_id;
            result.push_back(property::CHANNEL_ICON_ID);
        }
    }
    /* update the channel talk power */
    {
        permission::PermissionValue talk_power{0};
        auto& permission_tp_flags = permission_manager->permission_flags(permission::i_client_needed_talk_power);
        if(permission_tp_flags.value_set)
            talk_power = permission_manager->permission_values(permission::i_client_needed_talk_power).value;
        if(this->properties()[property::CHANNEL_NEEDED_TALK_POWER] != talk_power) {
            this->properties()[property::CHANNEL_NEEDED_TALK_POWER] = talk_power;
            result.push_back(property::CHANNEL_NEEDED_TALK_POWER);
        }
    }

    return result;
}

std::shared_ptr<BasicChannel> BasicChannel::parent() {
    auto link_lock = this->_link.lock();
    if(!link_lock)
        return nullptr;

    auto parent_lock = link_lock->parent.lock();
    if(!parent_lock)
        return nullptr;

    return dynamic_pointer_cast<BasicChannel>(parent_lock->entry);
}

BasicChannel::BasicChannel(std::shared_ptr<BasicChannel> parent, ChannelId channelId) : BasicChannel(parent ? parent->channelId() : 0, channelId) { }

BasicChannel::~BasicChannel() { }

ChannelType::ChannelType BasicChannel::channelType() {
    if (this->properties()[property::CHANNEL_FLAG_PERMANENT].as<bool>()) return ChannelType::ChannelType::permanent;
    else if (this->properties()[property::CHANNEL_FLAG_SEMI_PERMANENT].as<bool>()) return ChannelType::ChannelType::semipermanent;
    else return ChannelType::ChannelType::temporary;
}

void BasicChannel::setChannelType(ChannelType::ChannelType type) {
    properties()[property::CHANNEL_FLAG_PERMANENT] = type == ChannelType::permanent;
    properties()[property::CHANNEL_FLAG_SEMI_PERMANENT] = type == ChannelType::semipermanent;
}

bool BasicChannel::passwordMatch(std::string password, bool hashed) {
    if (!this->properties()[property::CHANNEL_FLAG_PASSWORD].as<bool>() || this->properties()[property::CHANNEL_PASSWORD].value().empty()) return true;
    if (password.empty()) return false;

    if(this->properties()[property::CHANNEL_PASSWORD].as<string>() == password)
        return true;

    password = base64::encode(digest::sha1(password));
    return this->properties()[property::CHANNEL_PASSWORD].as<string>() == password;
}

int64_t BasicChannel::emptySince() {
    if (!properties().hasProperty(property::CHANNEL_LAST_LEFT))
        return 0;

    time_point<system_clock> lastLeft = time_point<system_clock>() + milliseconds(properties()[property::CHANNEL_LAST_LEFT].as<uint64_t>());
    return (int64_t) duration_cast<seconds>(system_clock::now() - lastLeft).count();
}

void BasicChannel::setLinkedHandle(const std::weak_ptr<TreeView::LinkedTreeEntry> &ptr) {
    TreeEntry::setLinkedHandle(ptr);
    this->_link = ptr;
}

ChannelId BasicChannel::channelId() const {
    return this->_channel_id;
}

ChannelId BasicChannel::previousChannelId() const {
    return this->_channel_order;
}

void BasicChannel::setPreviousChannelId(ChannelId id) {
    this->properties()[property::CHANNEL_ORDER] = id;
}

void BasicChannel::setParentChannelId(ChannelId id) {
    this->properties()[property::CHANNEL_PID] = id;
}

BasicChannelTree::BasicChannelTree() { }

BasicChannelTree::~BasicChannelTree() { }

std::deque<std::shared_ptr<BasicChannel>> BasicChannelTree::channels(const shared_ptr<BasicChannel> &root, int deep) {
    auto result = root ? this->entries_sub(root, deep) : this->entries(root, deep);

    std::deque<std::shared_ptr<BasicChannel>> res;
    for(const auto& entry : result) {
        auto e = dynamic_pointer_cast<BasicChannel>(entry);
        if(e) res.push_back(e);
    }
    return res;
}

shared_ptr<TreeView::LinkedTreeEntry> BasicChannelTree::findLinkedChannel(ts::ChannelId channelId) {
    return this->find_linked_entry(channelId);
}

std::shared_ptr<BasicChannel> BasicChannelTree::findChannel(ChannelId channelId) {
    if(channelId == 0) return nullptr; /* we start counting at 1! */
    return this->findChannel(channelId, this->channels());
}

std::shared_ptr<BasicChannel> BasicChannelTree::findChannel(ChannelId channelId, std::deque<std::shared_ptr<BasicChannel>> avariable) {
    for (auto elm : avariable)
        if (elm->channelId() == channelId)
            return elm;
    return nullptr;
}


std::shared_ptr<BasicChannel> BasicChannelTree::findChannel(const std::string &name, const shared_ptr<BasicChannel> &layer) {
    for (auto elm : this->channels())
        if (elm->name() == name && elm->parent() == layer)
            return elm;
    return nullptr;
}

std::shared_ptr<BasicChannel> BasicChannelTree::findChannelByPath(const std::string &path) {
    int maxChannelDeep = 255; //FIXME

    std::deque<std::string> entries;
    size_t index = 0;
    do {
        auto found = path.find('/', index);
        if (found == index) {
            index++;
            continue;
        } else if (found < path.length() && path[found - 1] == '\\') {
            index++;
            continue;
        }
        entries.push_back(query::unescape(path.substr(index, found - index), false));
        index = found + 1;
    } while (index != 0 && entries.size() <= maxChannelDeep);

    debugMessage(LOG_GENERAL, "Parsed channel path \"{}\". Entries:", path);
    std::shared_ptr<BasicChannel> current = nullptr;
    for (const auto &name : entries) {
        current = this->findChannel(name, current);
        debugMessage(LOG_GENERAL, " - \"{}\" {}", name, (current ? "found" : "unknown"));
        if (!current) break;
    }
    return current;
}

std::shared_ptr<BasicChannel> BasicChannelTree::createChannel(ChannelId parentId, ChannelId orderId, const string &name) {
    auto parent = this->findChannel(parentId);
    if (!parent && parentId != 0) return nullptr;

    auto order = this->findChannel(orderId);
    if (!order && orderId != 0) return nullptr;


    if (this->findChannel(name, parent)) return nullptr;

    auto channelId = generateChannelId();
    if (channelId < 1) {
        logger::logger(0)->error("Cant generate channel id.");
        return nullptr;
    }

    auto channel = this->allocateChannel(parent, channelId);
    channel->properties()[property::CHANNEL_NAME] = name;

    if(!this->insert_entry(channel, parent, order)) return nullptr;
    return channel;
}

deque<std::shared_ptr<ts::BasicChannel>> BasicChannelTree::delete_channel_root(const std::shared_ptr<ts::BasicChannel> &root) {
    deque<std::shared_ptr<ts::BasicChannel>> result;
    /*
    for(const auto& channels : this->channels(root)) {
        std::lock_guard lock(channels->deleteLock);
        channels->deleted = true;
    }
     */
    auto channels = this->delete_entry(root);
    for(const auto& channel : channels) {
        if(!channel) continue;
        auto c = dynamic_pointer_cast<BasicChannel>(channel);
        assert(c);
        this->on_channel_entry_deleted(c);
        result.push_back(c);
    }

    return result;
}

bool BasicChannelTree::setDefaultChannel(const shared_ptr<BasicChannel> &ch) {
    if (!ch) return false;
    for (const auto &elm : this->channels())
        elm->properties()[property::CHANNEL_FLAG_DEFAULT] = false;
    ch->properties()[property::CHANNEL_FLAG_DEFAULT] = true;
    return true;
}

std::shared_ptr<BasicChannel> BasicChannelTree::getDefaultChannel() {
    for (auto elm : this->channels())
        if (elm->properties()[property::CHANNEL_FLAG_DEFAULT].as<bool>()) return elm;
    return nullptr;
}

void BasicChannelTree::on_channel_entry_deleted(const shared_ptr<BasicChannel> &ptr) {

}

std::shared_ptr<BasicChannel> BasicChannelTree::allocateChannel(const shared_ptr<BasicChannel> &parent, ChannelId channelId) {
    return std::make_shared<BasicChannel>(parent, channelId);
}

ChannelId BasicChannelTree::generateChannelId() {
    ChannelId currentChannelId = 1;
    while (this->findChannel(currentChannelId)) currentChannelId++;
    return currentChannelId;
}

void BasicChannelTree::printChannelTree(std::function<void(std::string)> fn) {
    this->print_tree([&](const std::shared_ptr<TreeEntry>& t_entry, int deep) {
        auto entry = dynamic_pointer_cast<BasicChannel>(t_entry);
        assert(entry);

        string prefix;
        while(deep-- > 0) prefix += "  ";

        fn(prefix + " - " + to_string(entry->channelId()) + " | " + to_string(entry->previousChannelId()) + " (" + entry->name() + ")");
    });
}