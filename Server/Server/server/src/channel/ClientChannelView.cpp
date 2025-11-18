#include <misc/sassert.h>
#include <log/LogUtils.h>
#include <misc/memtracker.h>
#include "src/client/ConnectedClient.h"
#include "ClientChannelView.h"

using namespace std;
using namespace ts;
using namespace ts::server;

ViewEntry::ViewEntry(const std::shared_ptr<ts::BasicChannel> &handle, bool editable) : handle(handle), editable(editable) {
    memtrack::allocated<ViewEntry>(this);
    assert(handle);

    this->view_timestamp = chrono::system_clock::now();
    this->previous_channel = handle->channelOrder();
    this->cached_channel_id = handle->channelId();
    this->cached_parent_id = handle->hasParent() ? handle->parent()->channelId() : 0;
}

ViewEntry::~ViewEntry() {
    memtrack::freed<ViewEntry>(this);
}

ChannelId ts::ViewEntry::channelId() const {
    if(this->cached_channel_id != 0) return this->cached_channel_id; //We've already got a channel id

    //TODO cached_channel_id should be > 0 every time?
    auto channel = this->handle.lock();
    if(!channel) {
        logCritical(LOG_GENERAL, "ViewEntry::channelId() called without a valid handle and cached_channel_id == 0!");
        return 0;
    }
    return channel->channelId();
}

ChannelId ViewEntry::parentId() const {
    if(this->cached_parent_id != 0) return this->cached_parent_id;

    auto channel = this->handle.lock();
    return channel && channel->parent() ? channel->parent()->channelId() : 0;
}

ChannelId ts::ViewEntry::previousChannelId() const {
    return previous_channel;
}

void ts::ViewEntry::setPreviousChannelId(ts::ChannelId id) {
    assert(this->editable);
    this->previous_channel = id;
}

void ts::ViewEntry::setParentChannelId(ts::ChannelId id) {
    assert(this->editable);
    this->cached_parent_id = id;
}

ClientChannelView::ClientChannelView(server::ConnectedClient* handle) : owner(handle) {
    memtrack::allocated<ClientChannelView>(this);
}
ClientChannelView::~ClientChannelView() {
    memtrack::freed<ClientChannelView>(this);
}

ServerId ClientChannelView::getServerId() {
    return owner ? owner->getServerId() : 0;
}

std::deque<std::shared_ptr<BasicChannel>> ClientChannelView::channels(const std::shared_ptr<ts::BasicChannel> &head,
                                                                      int deep) {
    std::deque<std::shared_ptr<BasicChannel>> result;
    for(const auto& entry : this->entries(head ? make_shared<ViewEntry>(head) : nullptr, deep)) {
        auto channel = dynamic_pointer_cast<ViewEntry>(entry)->handle.lock();
        if(channel)
            result.push_back(channel);
    }

    return result;
}

bool ClientChannelView::channel_visible(const std::shared_ptr<ts::BasicChannel> &channel,
                                                                             const std::shared_ptr<ts::BasicChannel> &head,
                                                                             int deep) {
    if(!channel) return true; //I thing the void is kind of visible :D
    return this->has_entry(make_shared<ViewEntry>(channel), head ? make_shared<ViewEntry>(head) : nullptr, deep);
}

std::shared_ptr<ViewEntry> ClientChannelView::find_channel(ts::ChannelId id) {
    return dynamic_pointer_cast<ViewEntry>(this->find_entry(id));
}

std::shared_ptr<ViewEntry> ClientChannelView::find_channel(const std::shared_ptr<ts::BasicChannel> &channel) {
    if(!channel) return nullptr;

    deque<shared_ptr<BasicChannel>> heads{channel};
    shared_ptr<LinkedTreeEntry> head = nullptr;
    bool deep_search = false;

    while(heads.front()) {
        auto parent = heads.front()->parent();
        if(!parent && heads.front()->properties()[property::CHANNEL_PID].as_or<ChannelId>(0) != 0) {
            head = this->find_linked_entry(channel->channelId(), nullptr);//We're searching for a deleted head! So lets iterate over everything
            deep_search = true;
            break;
        }
        heads.push_front(parent);
    }

    if(!deep_search) {
        heads.pop_front();

        while(heads.size() > 1) {
            auto front = move(heads.front());
            heads.pop_front();

            head = this->find_linked_entry(front->channelId(), head, 1);
            if(!head) return nullptr; //Channel tree not visible!
        }

        head = this->find_linked_entry(channel->channelId(), head, 1);
    }

    return head ? static_pointer_cast<ViewEntry>(head->entry) : nullptr;
}

std::deque<std::shared_ptr<ViewEntry>> ClientChannelView::insert_channels(shared_ptr<TreeView::LinkedTreeEntry> head, bool test_permissions, bool first_only) {
    std::deque<std::shared_ptr<ViewEntry>> result;

    bool has_perm = !test_permissions || permission::v2::permission_granted(1, owner->calculate_permission(permission::b_channel_ignore_view_power, 0, false));
    bool first = true;
    while(head) {
        if(!first && first_only) break;
        first = false;

        auto channel = dynamic_pointer_cast<BasicChannel>(head->entry);
        if(this->channel_visible(channel)) {
            if(head->child_head) {
                for(const auto& sub : this->insert_channels(head->child_head, test_permissions, false))
                    result.push_back(sub);
            }

            head = head->next;
            continue;
        }

        if(!has_perm) {
            if(!channel->permission_granted(permission::i_channel_needed_view_power, this->owner->calculate_permission(permission::i_channel_view_power, channel->channelId()), false)) {
                head = head->next;
                debugMessage(this->getServerId(), "{}[CHANNEL] Dropping channel {} ({}) (No permissions)", CLIENT_STR_LOG_PREFIX_(this->owner), channel->channelId(), channel->name());
                continue;
            }
        }
        auto entry = make_shared<ViewEntry>(dynamic_pointer_cast<BasicChannel>(head->entry), true);
        std::shared_ptr<ViewEntry> parent, previous;

        if(head->parent.lock()) {
            auto remote_parent = head->parent.lock();
            parent = dynamic_pointer_cast<ViewEntry>(this->find_entry(remote_parent->entry->channelId()));
            sassert(parent);
        }

        auto remote_previous = head->previous; //Get our first thing
        while(remote_previous && !(previous = dynamic_pointer_cast<ViewEntry>(this->find_entry(remote_previous->entry->channelId())))) {
            remote_previous = remote_previous->previous;
        }

        auto previous_channel = previous ? previous->channel() : nullptr;
        if(!this->insert_entry(entry, parent, previous)) {
            logError(this->getServerId(), "Failed to insert channel into client view!");
            head = head->next;
            continue;
        };
        auto now_prv = this->find_channel(entry->previousChannelId());
        logTrace(this->getServerId(), "{}[CHANNELS] Insert channel {} ({}) after {} ({}). Original view prv: {} ({}). Original prv: {} ({})",
                     CLIENT_STR_LOG_PREFIX_(this->owner),
                     channel->channelId(), channel->name(),
                     entry->previousChannelId(), now_prv ? now_prv->channel()->name() : "",
                     remote_previous ? remote_previous->entry->channelId() : 0, remote_previous ? dynamic_pointer_cast<BasicChannel>(remote_previous->entry)->name() : "",
                     head->previous ? head->previous->entry->channelId() : 0, head->previous ? dynamic_pointer_cast<BasicChannel>(head->previous->entry)->name() : ""
        );

        result.push_back(entry);

        if(head->child_head) {
            for(const auto& sub : this->insert_channels(head->child_head, test_permissions, false))
                result.push_back(sub);
        }
        head = head->next;
    }

    return result;
}

std::deque<std::shared_ptr<ViewEntry>> ClientChannelView::show_channel(std::shared_ptr<ts::TreeView::LinkedTreeEntry> l_channel, bool& success) {
    success = true;
    if(this->channel_visible(dynamic_pointer_cast<BasicChannel>(l_channel->entry))) return {};

    std::deque<std::shared_ptr<ViewEntry>> result;
    deque<shared_ptr<TreeView::LinkedTreeEntry>> parents = {l_channel};
    while(parents.front()) {
        auto parent = parents.front()->parent.lock();
        if(parent && !this->channel_visible(dynamic_pointer_cast<BasicChannel>(parent->entry))) {
            parents.push_front(parent);
        } else {
            parents.push_front(nullptr);
            break;
        }
    }
    parents.pop_front();

    for(const auto& root_channel : parents) {
        auto channel = dynamic_pointer_cast<BasicChannel>(root_channel->entry);
        auto entry = make_shared<ViewEntry>(channel, true);
        std::shared_ptr<ViewEntry> parent, previous;

        if(root_channel->parent.lock()) {
            auto remote_parent = root_channel->parent.lock();
            parent = dynamic_pointer_cast<ViewEntry>(this->find_entry(remote_parent->entry->channelId()));
            sassert(parent);
        }

        auto remote_previous = root_channel->previous; //Get our first thing
        while(remote_previous && !(previous = dynamic_pointer_cast<ViewEntry>(this->find_entry(remote_previous->entry->channelId())))) {
            remote_previous = remote_previous->previous;
        }
        auto previous_channel = previous ? previous->channel() : nullptr; //weak could be may nullptr
        logTrace(this->getServerId(), "{}[CHANNELS] Insert channel {} ({}) after {} ({})",
                     CLIENT_STR_LOG_PREFIX_(this->owner),
                     channel->channelId(), channel->name(),
                     previous ? previous->channelId() : 0, previous_channel ? previous_channel->name() : ""
        );

        if(!this->insert_entry(entry, parent, previous)) {
            logError(this->getServerId(), "Failed to insert channel into client view! (Aborting root)");
            success = false;
            break;
        };

        result.push_back(entry);
    }

    return result;
}

std::deque<std::shared_ptr<ViewEntry>> ClientChannelView::test_channel(std::shared_ptr<ts::TreeView::LinkedTreeEntry> l_old,
                                                                       std::shared_ptr<ts::TreeView::LinkedTreeEntry> channel_new) {
    std::deque<std::shared_ptr<ViewEntry>> result;
    bool has_perm = permission::v2::permission_granted(1, owner->calculate_permission(permission::b_channel_ignore_view_power, 0, false));
    if(has_perm) return {};

    deque<shared_ptr<TreeView::LinkedTreeEntry>> parents = {l_old};
    while(parents.front()) {
        auto parent = parents.front();
        auto current = channel_new;
        while(current && current != parent) current = current->parent.lock();
        if(current == parent) {
            //parents.push_front(nullptr);
            break;
        }
        parents.push_front(parent->parent.lock());
    }
    parents.pop_front();

    for(const auto& l_entry : parents) {
        auto entry = this->find_entry(l_entry->entry->channelId());
        if(!entry) break; //Already cut out!
        auto channel = dynamic_pointer_cast<BasicChannel>(l_entry->entry);
        sassert(entry->channelId() == channel->channelId());

        if(!channel->permission_granted(permission::i_channel_needed_view_power, this->owner->calculate_permission(permission::i_channel_view_power, channel->channelId()), false)) {

            for(const auto& te : this->delete_entry(entry))
                result.push_back(dynamic_pointer_cast<ViewEntry>(te));

            debugMessage(this->getServerId(), "{}[CHANNEL] Moving channel tree out of view. Root: {} ({}) (No permissions)", CLIENT_STR_LOG_PREFIX_(this->owner), channel->channelId(), channel->name());
            break;
        }
    }

    return result;
}

std::deque<std::pair<bool, std::shared_ptr<ViewEntry>>> ClientChannelView::update_channel(
        std::shared_ptr<ts::TreeView::LinkedTreeEntry> l_channel, std::shared_ptr<ts::TreeView::LinkedTreeEntry> l_own) {
    return update_channel_path(std::move(l_channel), std::move(l_own), 1);
}

std::deque<std::pair<bool, std::shared_ptr<ViewEntry>>> ClientChannelView::update_channel_path(std::shared_ptr<ts::TreeView::LinkedTreeEntry> l_channel, std::shared_ptr<ts::TreeView::LinkedTreeEntry> l_own, ssize_t length) {
    std::deque<std::pair<bool, std::shared_ptr<ViewEntry>>> result;
    bool has_perm = permission::v2::permission_granted(1, owner->calculate_permission(permission::b_channel_ignore_view_power, 0, false));

    while(l_channel && length-- != 0) {
        auto b_channel = dynamic_pointer_cast<BasicChannel>(l_channel->entry);
        sassert(b_channel);
        auto visible = this->channel_visible(b_channel);
        if(!visible) {
            if(l_channel->parent.lock() && !this->channel_visible(dynamic_pointer_cast<BasicChannel>(l_channel->parent.lock()->entry))) {
                l_channel = l_channel->next;
                continue; /* all subchannels had been checked, because parent isnt visible */
            }
            //Test if channel comes visible again!
            visible = true;

            if(!has_perm) {
                if(!b_channel->permission_granted(permission::i_channel_needed_view_power, this->owner->calculate_permission(permission::i_channel_view_power, b_channel->channelId()), false)) {
                    visible = false;
                }
            }
            if(visible) {
                for(const auto& entry : this->show_channel(l_channel, visible))
                    result.emplace_back(true, entry);
                for(const auto& entry : this->insert_channels(l_channel->child_head, true, false))
                    result.emplace_back(true, entry);
            }

            l_channel = l_channel->next;
            continue; /* all subchannels had been checked */
        } else if(visible && !has_perm) {
            for(const auto& entry : this->test_channel(l_channel, l_own))
                result.emplace_back(false, entry);
        }

        //Root node is okey, test children
        if(l_channel->child_head) {
            auto entries = this->update_channel_path(l_channel->child_head, l_own, -1);
            result.insert(result.end(), entries.begin(), entries.end());
        }


        l_channel = l_channel->next;
    }

    return result;
}

std::deque<std::pair<ClientChannelView::ChannelAction, std::shared_ptr<ViewEntry>>> ClientChannelView::change_order(const shared_ptr<LinkedTreeEntry> &channel, const std::shared_ptr<LinkedTreeEntry> &parent, const shared_ptr<LinkedTreeEntry> &head) {
    std::deque<std::pair<ClientChannelView::ChannelAction, std::shared_ptr<ViewEntry>>> result;
    auto l_entry = this->find_linked_entry(channel->entry->channelId());
    auto l_parent = parent ? this->find_linked_entry(parent->entry->channelId()) : nullptr;
    if(!l_entry) { //Channel not visible yet
        if(!l_parent && parent) return {}; //The invisible channel was moved into an invisible tree

        bool has_perm = permission::v2::permission_granted(1, owner->calculate_permission(permission::b_channel_ignore_view_power, 0, false));
        if(!has_perm) {
            has_perm = dynamic_pointer_cast<BasicChannel>(channel->entry)->permission_granted(permission::i_channel_needed_view_power, this->owner->calculate_permission(permission::i_channel_view_power, dynamic_pointer_cast<BasicChannel>(channel->entry)->channelId()), false);
        }
        if(!has_perm) return {}; //Channel wasn't visible and he still has no permission for that :)

        std::shared_ptr<ViewEntry> previous;
        auto remote_previous = head; //Get our first thing
        while(remote_previous && !(previous = dynamic_pointer_cast<ViewEntry>(this->find_entry(remote_previous->entry->channelId())))) {
            remote_previous = remote_previous->previous;
        }

        for(const auto& shown : this->insert_channels(channel, true, true))
            result.push_back({ClientChannelView::ENTER_VIEW, shown});
        return result; //An invisible channel became visible
    }
    //Channel visible!

    if(!l_parent && parent) { //Channel was visible and moved to invisible tree
        auto remove = this->delete_entry(l_entry->entry);
        for(const auto& entry : remove)
            result.push_back({ClientChannelView::DELETE_VIEW, dynamic_pointer_cast<ViewEntry>(entry)});
        return result;
    }

    //We have just to readjust the order or the parent
    std::shared_ptr<ViewEntry> previous;
    auto remote_previous = head; //Get our first thing
    while(remote_previous && !(previous = dynamic_pointer_cast<ViewEntry>(this->find_entry(remote_previous->entry->channelId())))) {
        remote_previous = remote_previous->previous;
    }

    auto parent_switch = l_parent != l_entry->parent.lock();
    if(!this->move_entry(l_entry->entry, l_parent ? l_parent->entry : nullptr, previous)) return {};

    return {{parent_switch ? ClientChannelView::MOVE : ClientChannelView::REORDER, dynamic_pointer_cast<ViewEntry>(l_entry->entry)}};
}

std::shared_ptr<ViewEntry> ClientChannelView::add_channel(const std::shared_ptr<ts::TreeView::LinkedTreeEntry>& l_channel) {
    auto l_parent_channel = l_channel->parent.lock();
    auto parent_channel = l_parent_channel ? this->find_linked_entry(l_parent_channel->entry->channelId()) : nullptr;
    if(!parent_channel && l_parent_channel) return nullptr; //Tree not visible!

    shared_ptr<ViewEntry> previous;
    auto remote_previous = l_channel->previous; //Get our first thing
    while(remote_previous && !(previous = dynamic_pointer_cast<ViewEntry>(this->find_entry(remote_previous->entry->channelId())))) {
        remote_previous = remote_previous->previous;
    }

    auto entry = make_shared<ViewEntry>(dynamic_pointer_cast<BasicChannel>(l_channel->entry), true);
    if(!this->insert_entry(entry, parent_channel ? parent_channel->entry : nullptr, previous)) return nullptr;
    return entry;
}

bool ClientChannelView::remove_channel(ts::ChannelId channelId) {
    auto entry = this->find_entry(channelId);
    if(!entry)
        return false;

    auto result = this->delete_entry(entry);
    if(result.size() != 1) {
        logError(this->owner->getServerId(), "ClientChannelView::remove_channel(...) returned more than one channel! ({})", result.size());
        for(const auto& entry : result)
            debugMessage(this->owner->getServerId(), " - {}", entry->channelId());
    }
    return true;
}

std::deque<ChannelId> ClientChannelView::delete_channel_root(const std::shared_ptr<ts::BasicChannel> &channel){
    auto linked = this->find_channel(channel);
    if(!linked) return {};

    std::deque<ChannelId> result;
    for(const auto& channel : this->delete_entry(linked))
        result.push_back(channel->channelId());
    return result;
}

void ClientChannelView::reset() {
    while(this->head)
        this->delete_entry(this->head->entry);
}

void ClientChannelView::print() {
    debugMessage(this->owner->getServerId(), "{}'s channel tree: ", this->owner->getDisplayName());
    this->print_tree([&](const std::shared_ptr<TreeEntry>& entry, int deep) {
        string prefix;
        while(deep > 0) {
            prefix += "  ";
            deep--;
        }

        debugMessage(this->owner->getServerId(), "{} - {} ({})", prefix, entry->channelId(), entry->previousChannelId());
    });
}
