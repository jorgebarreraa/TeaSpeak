#pragma once

#include <utility>
#include <functional>

#include "misc/advanced_mutex.h"
#include <Definitions.h>
#include <deque>
#include <ThreadPool/Mutex.h>
#include "misc/memtracker.h"

#ifndef __attribute_deprecated__
    #define __attribute_deprecated__ [[deprecated]]
#endif

namespace ts {
    class TreeEntry;
    class TreeView {
        public:
        struct LinkedTreeEntry {
            std::shared_ptr<LinkedTreeEntry> previous;
            std::shared_ptr<LinkedTreeEntry> next;
            std::shared_ptr<LinkedTreeEntry> child_head;
            std::weak_ptr<LinkedTreeEntry> parent;

            const std::shared_ptr<TreeEntry> entry;

            explicit LinkedTreeEntry(std::shared_ptr<TreeEntry> entry) : entry(std::move(entry)) {
                memtrack::allocated<LinkedTreeEntry>(this);
            }

            virtual ~LinkedTreeEntry() {
                memtrack::freed<LinkedTreeEntry>(this);
            }
        };

        public:
            TreeView();
            virtual ~TreeView();

            [[nodiscard]] size_t entry_count() const;
            [[nodiscard]] std::deque<std::shared_ptr<TreeEntry>> entries(const std::shared_ptr<TreeEntry>& /* head */ = nullptr, int /* deep */ = -1) const;
            [[nodiscard]] std::deque<std::shared_ptr<TreeEntry>> entries_sub(const std::shared_ptr<TreeEntry>& /* parent */ = nullptr, int /* deep */ = -1) const;
            [[nodiscard]] std::shared_ptr<TreeEntry> find_entry(ChannelId /* channel id */) const;
            [[nodiscard]] std::shared_ptr<LinkedTreeEntry> find_linked_entry(ChannelId /* channel id */, const std::shared_ptr<LinkedTreeEntry>& /* head */ = nullptr, int deep = -1) const;

            bool has_entry(const std::shared_ptr<TreeEntry>& /* entry */, const std::shared_ptr<TreeEntry>& /* head */ = nullptr, int deep = -1) const;

            bool insert_entry(const std::shared_ptr<TreeEntry>& entry, const std::shared_ptr<TreeEntry>& /* parent */ = nullptr, const std::shared_ptr<TreeEntry>& /* previous */ = nullptr);
            bool move_entry(const std::shared_ptr<TreeEntry>& /* entry */, const std::shared_ptr<TreeEntry>& /* parent */ = nullptr, const std::shared_ptr<TreeEntry>& /* previous */ = nullptr);
            std::deque<std::shared_ptr<TreeEntry>> delete_entry(std::shared_ptr<TreeEntry> /* entry */); //Copy that here because of reference could be changed (linked->entry)

            void print_tree(const std::function<void(const std::shared_ptr<TreeEntry>& /* entry */, int /* deep */)>&) const;
        protected:
            std::shared_ptr<LinkedTreeEntry> head;
        private:
            inline std::shared_ptr<LinkedTreeEntry> linked(const std::shared_ptr<TreeEntry>& /* entry */) const;
            inline std::deque<std::shared_ptr<LinkedTreeEntry>> query_deep(const std::shared_ptr<LinkedTreeEntry>& /* layer */ = nullptr,int /* max deep */ = -1) const;
            inline void query_deep_(std::deque<std::shared_ptr<LinkedTreeEntry>>& /* result */, const std::shared_ptr<LinkedTreeEntry>& /* layer */ = nullptr,int /* max deep */ = -1) const;
            inline std::deque<std::shared_ptr<TreeEntry>> query_deep_entry(const std::shared_ptr<LinkedTreeEntry>& /* layer */ = nullptr,int /* max deep */ = -1) const;
            inline bool has_entry_(const std::shared_ptr<TreeEntry>& /* entry */, const std::shared_ptr<LinkedTreeEntry>& /* head */ = nullptr, int deep = -1) const;
            void print_tree_(const std::function<void(const std::shared_ptr<TreeEntry>& /* entry */, int /* deep */)>&, std::shared_ptr<LinkedTreeEntry> /* head */, int /* deep */) const;

            inline void cut_entry(const std::shared_ptr<LinkedTreeEntry>&);
    };

    class TreeEntry {
        public:
            virtual ChannelId channelId() const = 0;

            virtual ChannelId previousChannelId() const = 0;
            virtual void setPreviousChannelId(ChannelId) = 0;
            virtual void setParentChannelId(ChannelId) = 0;

            virtual void setLinkedHandle(const std::weak_ptr<TreeView::LinkedTreeEntry>& /* self */) {}
    };
}