
#include <src/query/Command.h>
#include <src/BasicChannel.h>
#include <ThreadPool/Thread.h>
#include "channel/TreeView.h"

using namespace std;
using namespace std::chrono;
using namespace ts;

struct TEntry : public ts::TreeEntry {
    public:
        TEntry(ChannelId channel_id) : channel_id(channel_id), previous_id(0) {}

        ChannelId channelId() const override {
            return channel_id;
        }

        ChannelId previousChannelId() const override {
            return previous_id;
        }

        void setParentChannelId(ChannelId id) override {

        }

        void setPreviousChannelId(ChannelId id) override {
            previous_id = id;
        }

        bool deleted() const override {
            return _deleted;
        }

        void set_deleted(bool b) override {
            _deleted = b;
        }

        bool _deleted = false;
        ChannelId channel_id;
        ChannelId previous_id;
};

void tree_print_entry(const std::shared_ptr<TreeEntry>& t_entry, int deep) {
    auto entry = dynamic_pointer_cast<TEntry>(t_entry);
    assert(entry);

    string prefix;
    while(deep-- > 0) prefix += "  ";

    cout << prefix <<  "- " << entry->channel_id << endl;
}


#define PT                                      \
cout << " --------- TREE --------- " << endl;   \
tree.print_tree(tree_print_entry);              \
cout << " --------- TREE --------- " << endl;

template <typename T>
void print_address(const T& idx) {
    cout << &idx << endl;
    [idx]() {
        cout << &idx << endl;
    }();
}

int main() {
    auto index = shared_ptr<int>();
    print_address(index);
    return 0;
    /*
    BasicChannelTree tree;
    auto ch3 = tree.createChannel(0, 0, "test channel");
    auto ch2 = tree.createChannel(0, 0, "test channel2");
    auto ch = tree.createChannel(ch2->channelId(), 0, "test channel 2");
    tree.deleteChannelRoot(ch2);
    ch2 = nullptr;
    ch = nullptr;

    threads::self::sleep_for(seconds(1));
    cout << "XX" << endl;
     */
    ChannelId channel_id_index = 0;

    TreeView tree;

    /* Create 10 channels */
    while(channel_id_index < 20)
        assert(tree.insert_entry(make_shared<TEntry>(channel_id_index++)));
    PT

    /* Test order id */
    for(int i = 0; i < 10000; i++) { //Random test
        auto channel_id = tree.find_entry(rand() % channel_id_index);
        auto channel_target = tree.find_entry(rand() % channel_id_index);
        assert(channel_id);
        assert(channel_target);
        printf("Move channel %lu after %lu => ", channel_id->channelId(), channel_target->channelId());
        printf("%x\n", tree.move_entry(channel_id, nullptr, channel_target));
    }
    PT

    /* Test move */
    for(int i = 0; i < 10002; i++) { //Random test
        auto channel_id = tree.find_entry(rand() % channel_id_index);
        auto channel_target = tree.find_entry(rand() % channel_id_index);
        printf("Move channel parent of %lu to %lu => ", channel_id->channelId(), channel_target->channelId());
        printf("%x\n", tree.move_entry(channel_id, channel_target));
    }
    PT

    memtrack::statistics();

    for(int i = 0; i < 20; i++) {
        tree.delete_entry(tree.find_entry(rand() % channel_id_index));
        PT
        memtrack::statistics();
    }
    return 0;
}