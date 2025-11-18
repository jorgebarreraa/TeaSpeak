#include <unordered_map>
#include <algorithm>
#include <string>
#include "linked_helper.h"

using namespace std;
using namespace linked;

std::shared_ptr<entry> linked::build_chain(const std::deque<std::shared_ptr<linked::entry>> &entries, std::deque<std::string> &log) {
    //TODO filter only for one layer
    auto find_entry = [&](uint64_t id) -> shared_ptr<entry> {
        for(const auto& entry : entries)
            if(entry->entry_id == id)
                return entry;
        return nullptr;
    };

    deque<shared_ptr<entry>> heads;
    {
        //first linking
        for(const auto& entry : entries) {
            auto previous = find_entry(entry->previous_id);
            if(!previous && entry->previous_id > 0) {
                log.emplace_back("missing " + to_string(entry->entry_id) + "'s previous entry (" + to_string(entry->previous_id) + "). Removing previous");
                entry->previous_id = 0;
                entry->modified = true;
            }

            if(previous) {
                /* validate previous stuff */
                if((previous->next && previous->next != entry)) {
                    log.emplace_back(to_string(entry->entry_id) + "'s previous node has already someone linked (" + to_string(previous->next->entry_id) + "). Removing previous");
                    entry->previous_id = 0;
                    entry->modified = true;
                    previous = nullptr;
                } else if(previous == entry) {
                    log.emplace_back(to_string(entry->entry_id) + "'s previous node references to himself. Removing previous");
                    entry->previous_id = 0;
                    entry->modified = true;
                    previous = nullptr;
                }
            }
            if(previous) {
                previous->next = entry;
                entry->previous = previous;
            } else {
                heads.push_back(entry);
            }
        }
    }

    {
        /*
         * Now test for circles (the heads could not contain a circle because they have one open end)
         * But we've may nodes which are not within the heads
         */

        unordered_map<void*, uint8_t> used_nodes;
        deque<shared_ptr<entry>> unused_nodes;

        for(auto head : heads) {
            while(head) {
                auto& value = used_nodes[&*head];
                if(value) {
                    //Node has been already used
                    log.emplace_back(to_string(head->entry_id) + "'s has already been used, but is linked in another chain! We could not recover from that");
                    return nullptr;
                } else
                    value = 1;

                head = head->next;
            }
        }

        for(const auto& node : entries) {
            if(!used_nodes[&*node])
                unused_nodes.push_back(node);
        }

        while(!unused_nodes.empty()) {
            auto head = std::move(unused_nodes.front());
            unused_nodes.pop_front();

            log.emplace_back("Found circle. Cutting circle between " + to_string(head->previous->entry_id) + " and " + to_string(head->entry_id));
            if(head->previous->next == head)
                head->previous->next = nullptr;

            head->previous = nullptr;
            head->previous_id = 0;
            head->modified = true;

            heads.push_back(head);
            while(head->next) {
                auto it = find(unused_nodes.begin(), unused_nodes.end(), head->next);
                if(it != unused_nodes.end()) {
                    unused_nodes.erase(it);
                } else {
                    log.emplace_back(to_string(head->entry_id) + "'s has already been used, but is linked in another chain! We could not recover from that");
                    return nullptr;
                }
            }
        }

        if(heads.empty()) {
            if(!entries.empty())
                log.emplace_back("failed to detect heads! We could not recover from that");
            return nullptr;
        }

        auto head = heads.front();
        heads.pop_front();

        auto tail = head;
        while(tail->next) tail = tail->next;

        while(!heads.empty()) {

            auto local_head = heads.front();
            heads.pop_front();

            log.emplace_back("Appending open begin (" + to_string(local_head->entry_id) + ") to current tail (" + to_string(tail->entry_id) + ")");
            local_head->previous = tail;
            local_head->previous_id = tail->entry_id;
            local_head->modified = true;

            tail->next = local_head;

            while(tail->next) tail = tail->next; /* walk to the tail again */
        }

        return head;
    }
}