#pragma once

#include <memory>
#include <deque>

namespace linked {
    struct entry {
        std::shared_ptr<entry> previous;
        std::shared_ptr<entry> next;
        std::shared_ptr<entry> child_head;

        uint64_t parent_id;
        uint64_t entry_id;
        uint64_t previous_id;

        bool fully_linked = false;
        bool modified = false;
    };

    inline std::shared_ptr<entry> create_entry(uint64_t parent_id, uint64_t entry_id, uint64_t previous_id) {
        auto result = std::make_shared<entry>();
        result->parent_id = parent_id;
        result->entry_id = entry_id;
        result->previous_id = previous_id;

        return result;
    }

    extern std::shared_ptr<entry> build_chain(const std::deque<std::shared_ptr<entry>>& /* entried */, std::deque<std::string>& /* error log */);
}