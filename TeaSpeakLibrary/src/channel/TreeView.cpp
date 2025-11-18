#include <algorithm>
#include "TreeView.h"

using namespace ts;
using namespace std;
using LinkedTreeEntry = ts::TreeView::LinkedTreeEntry;

TreeView::TreeView() {}
TreeView::~TreeView() {
    std::deque<std::shared_ptr<LinkedTreeEntry>> heads = {this->head};
    while(!heads.empty()) {
        auto e = std::move(heads.front());
        heads.pop_front();

        while(e) {
            if(e->child_head)
                heads.push_back(e->child_head);

            //Release reference
            if(e->previous) e->previous->next = nullptr;
            e->previous = nullptr;
            e->child_head = nullptr;
            e = e->next;
        }
    }
}

std::shared_ptr<LinkedTreeEntry> TreeView::linked(const std::shared_ptr<ts::TreeEntry>& entry) const {
    if(!entry) return nullptr;

    std::deque<std::shared_ptr<LinkedTreeEntry>> heads = {this->head};
    while(!heads.empty()) {
        auto e = std::move(heads.front());
        heads.pop_front();

        while(e) {
            if(e->entry->channelId() == entry->channelId())
                return e;
            else {
                if(e->child_head)
                    heads.push_back(e->child_head);
                e = e->next;
            }
        }
    }

    return nullptr;
}

std::deque<std::shared_ptr<LinkedTreeEntry>> TreeView::query_deep(const std::shared_ptr<LinkedTreeEntry>& root, int deep) const {
    std::deque<std::shared_ptr<LinkedTreeEntry>> result;
    this->query_deep_(result, root ? root : this->head, deep);
    return result;
}

void TreeView::query_deep_(deque<shared_ptr<LinkedTreeEntry>>& result, const shared_ptr<LinkedTreeEntry>& root, int deep) const {
    if(deep == 0) return;

    shared_ptr<LinkedTreeEntry> entry = root;
    while(entry) {
        result.push_back(entry);
        if(entry->child_head)
            this->query_deep_(result, entry->child_head, deep - 1);
        entry = entry->next;
    }
}

size_t TreeView::entry_count() const {
    size_t result = 0;

    std::deque<std::shared_ptr<LinkedTreeEntry>> heads = {this->head};
    while(!heads.empty()) {
        auto e = std::move(heads.front());
        heads.pop_front();

        while(e) {
            result++;
            if(e->child_head)
                heads.push_back(e->child_head);
            e = e->next;
        }
    }

    return result;
}

std::shared_ptr<TreeEntry> TreeView::find_entry(ts::ChannelId channelId) const {
    auto result = this->find_linked_entry(channelId);
    return result ? result->entry : nullptr;
}

std::shared_ptr<LinkedTreeEntry> TreeView::find_linked_entry(ChannelId channelId, const std::shared_ptr<LinkedTreeEntry>& head, int deep) const {
    std::deque<std::shared_ptr<LinkedTreeEntry>> heads;
    heads.push_back(head ? head : this->head);


    while(!heads.empty()) {
        auto e = std::move(heads.front());
        heads.pop_front();

        while(e) {
            if(e->entry->channelId() == channelId)
                return e;
            if(e->child_head) {
                if(deep-- == 0) { //Reached max deep. Dont go deeper
                    deep++;
                } else {
                    if(e->next) heads.push_back(e->next);

                    e = e->child_head;
                    continue;
                }
            }
            e = e->next;
        }
        deep++;
    }

    return nullptr;
}

std::deque<std::shared_ptr<TreeEntry>> TreeView::entries(const std::shared_ptr<ts::TreeEntry>& root, int deep) const {
    return this->query_deep_entry(this->linked(root), deep);
}

std::deque<std::shared_ptr<TreeEntry>> TreeView::entries_sub(const std::shared_ptr<ts::TreeEntry> &root, int deep) const {
    auto l_root = this->linked(root);
    if(l_root && !l_root->child_head) return {root};

    auto result = this->query_deep_entry(l_root ? l_root->child_head : l_root, deep);
    result.push_back(root);
    return result;
}

std::deque<std::shared_ptr<TreeEntry>> TreeView::query_deep_entry(const shared_ptr<LinkedTreeEntry>& root, int deep) const {
    auto result = this->query_deep(root, deep);
    std::deque<std::shared_ptr<TreeEntry>> mapped;

    for(const auto& e : result)
        if(e->entry)
            mapped.push_back(e->entry);
    return mapped;
}

bool TreeView::has_entry(const std::shared_ptr<ts::TreeEntry> &entry, const std::shared_ptr<ts::TreeEntry> &root, int deep) const {
    auto l_root = this->linked(root);
    if(!l_root && root) return false;

    return this->has_entry_(entry, l_root ? l_root : this->head, deep);
}

bool TreeView::has_entry_(const shared_ptr<TreeEntry> &entry, const shared_ptr<LinkedTreeEntry> &head, int deep) const {
    shared_ptr<LinkedTreeEntry> element = head;
    while(element && deep != 0) {
        if(element->entry->channelId() == entry->channelId())
            return true;
        if(element->child_head)
            if(this->has_entry_(entry, element->child_head, deep - 1))
                return true;
        element = element->next;
    }
    return false;
}

bool TreeView::insert_entry(const shared_ptr<TreeEntry> &entry, const std::shared_ptr<TreeEntry> &t_parent, const shared_ptr<TreeEntry> &t_previous) {
    auto linked = make_shared<LinkedTreeEntry>(entry);
    linked->entry->setLinkedHandle(linked);

    /* Insert channel at the root at the back */
    if(!this->head) {
        this->head = linked;
        linked->entry->setPreviousChannelId(0);
        linked->entry->setParentChannelId(0);
        return true;
    }
    auto last = this->head;
    while(last->next) last = last->next;
    last->next = linked;
    linked->previous = last;

    if(!this->move_entry(entry, t_parent, t_previous)) {
        //FIXME delete it again
        return false;
    }
    return true;
}

bool TreeView::move_entry(const std::shared_ptr<ts::TreeEntry> &t_entry, const std::shared_ptr<ts::TreeEntry> &t_parent,
                          const std::shared_ptr<ts::TreeEntry> &t_previous) {
    if(t_entry == t_parent || t_entry == t_previous || (t_parent && t_parent == t_previous)) return false;

    auto entry = this->linked(t_entry);
    if(!entry) return false;

    auto parent = this->linked(t_parent);
    if(!parent && t_parent) return false;
    if(parent && entry->child_head) {
        auto childs = this->query_deep(entry->child_head);
        for(const auto& child : childs)
            if(child == parent) return false;
    }

    auto previous = this->linked(t_previous);
    if(!previous && t_previous) return false;

    if(previous && !this->has_entry_(t_previous, parent ? parent : this->head, 2)) return false; //Test if the t_parent channel contains t_previous

    /* cut the entry out */
    this->cut_entry(entry);
    entry->parent.reset();

    /* insert again */
    if(!this->head) {
        this->head = entry;
        entry->entry->setPreviousChannelId(0);
        return true;
    }
    entry->parent = parent;
    entry->entry->setParentChannelId(parent ? parent->entry->channelId() : 0);

    if(previous) {
        //Insert within the mid
        auto old_next = previous->next;

        //previous insert
        previous->next = entry;
        entry->previous = previous;
        entry->entry->setPreviousChannelId(previous->entry->channelId());

        //next insert
        entry->next = old_next;
        if(old_next) {
            old_next->previous = entry;
            old_next->entry->setPreviousChannelId(entry->entry->channelId());
        }
    } else {
        if(parent) {
            entry->next = parent->child_head;
            parent->child_head = entry;
        } else {
            entry->next = this->head;
            this->head = entry;
        }
        if(entry->next) {
            entry->next->previous = entry;
            entry->next->entry->setPreviousChannelId(entry->entry->channelId());
        }
        entry->entry->setPreviousChannelId(0);
    }

    return true;
}

void TreeView::cut_entry(const std::shared_ptr<LinkedTreeEntry>& entry) {
    if(this->head == entry) {
        this->head = entry->next;
        if(this->head) {
            this->head->previous = nullptr;
            this->head->entry->setPreviousChannelId(0);
        }
    } else {
        if(entry->previous) {
            assert(entry->previous->next == entry);
            entry->previous->next = entry->next;
        } else if(entry->parent.lock()) {
            auto e_parent = entry->parent.lock();
            assert(e_parent->child_head == entry);
            e_parent->child_head = entry->next;
        }
        if(entry->next) {
            assert(entry->next->previous == entry);

            entry->next->previous = entry->previous;
            entry->next->entry->setPreviousChannelId(entry->previous ? entry->previous->entry->channelId() : 0);
        }
    }
    entry->next = nullptr;
    entry->previous = nullptr;
}

std::deque<std::shared_ptr<TreeEntry>> TreeView::delete_entry(shared_ptr<TreeEntry> t_entry) {
    auto entry = this->linked(t_entry);
    if(!entry) return {};
    this->cut_entry(entry);

    std::deque<std::shared_ptr<TreeEntry>> result;
    std::deque<std::shared_ptr<LinkedTreeEntry>> heads = {entry};
    while(!heads.empty()) {
        auto e = std::move(heads.front());
        heads.pop_front();

        while(e) {
            if(e->child_head)
                heads.push_back(e->child_head);
            result.push_back(e->entry);

            //Release reference
            if(e->previous) e->previous->next = nullptr;
            e->previous = nullptr;
            e->child_head = nullptr;

            //e->entry = nullptr;
            if(e->entry->channelId() == t_entry->channelId()) {
                e = nullptr;
            } else {
                e = e->next;
            }
        }
    }
    std::reverse(result.begin(), result.end()); //Delete channels from the bottom to the top

    return result;
}

void TreeView::print_tree(const std::function<void(const std::shared_ptr<ts::TreeEntry> &, int)> &print) const {
    return this->print_tree_(print, this->head, 0);
}

void TreeView::print_tree_(const std::function<void(const std::shared_ptr<ts::TreeEntry> &, int)> &print, shared_ptr<LinkedTreeEntry> head, int deep) const {
    while(head) {
        print(head->entry, deep);
        this->print_tree_(print, head->child_head, deep + 1);
        head = head->next;
    }
}