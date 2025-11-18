#pragma once

#include <memory>
#include <cassert>
#include <chrono>
#include <deque>
#include <Definitions.h>
#include <mutex>
#include <vector>
#include <EventLoop.h>

namespace ts {
    namespace server {
        class VirtualServer;
        namespace conversation {
            struct ConversationEntry {
                std::chrono::system_clock::time_point message_timestamp;

                ClientDbId  sender_database_id;
                std::string sender_unique_id;
                std::string sender_name;

                std::string message;
                bool flag_message_deleted;
            };

            namespace fio {
                #pragma pack(push, 1)
                struct BlockHeader {
                    static constexpr uint64_t HEADER_COOKIE = 0xC0FFEEBABE;
                    static constexpr uint64_t MAX_BLOCK_SIZE = 0xFF00;

                    uint8_t version; /* every time 1 */
                    uint64_t cookie /* const 0xC0FFEEBABE */;
                    uint8_t message_version; /* every time 1; Version for the containing messages */

                    uint32_t block_size; /* size of the full block (with data) incl. header! */
                    uint32_t block_max_size; /* size of the full block incl. header! 0 if the block is located at the end and could be extended */

                    uint32_t message_count; /* message count */
                    uint32_t last_message_offset; /* offset to the last message. Offset begins after header (first message has offset of 0) */
                    union {
                        uint8_t flags;
                        struct {
                            uint8_t _padding : 5;

                            bool message_encrypted: 1; /* 0x04 */
                            bool meta_encrypted: 1; /* 0x02 */ /* Not implemented */
                            bool finished: 1; /* 0x01 */ /* if this block is finally finished; Most the time a next block follows directly */
                        };
                    };

                    uint64_t first_message_timestamp;
                    uint64_t last_message_timestamp;
                };
                static_assert(__BYTE_ORDER == __LITTLE_ENDIAN);
                static_assert(sizeof(BlockHeader) == 43);

                struct MessageHeader {
                    static constexpr uint16_t HEADER_COOKIE = 0xAFFE;
                    uint16_t cookie; /* const 0xAFFE */
                    uint16_t total_length; /* Total length of the full message data. Includes this header! */
                    uint64_t message_timestamp; /* milliseconds since epoch */
                    uint64_t sender_database_id;
                    uint8_t sender_unique_id_length; /* directly followed by this header */
                    uint8_t sender_name_length; /* directly followed after the unique id */
                    uint16_t message_length; /* directly followed after the name */
                    union {
                        uint16_t message_flags; /* could be later something like deleted etc.... */
                        struct {
                            uint16_t _flags_padding: 15;
                            bool flag_deleted: 1;
                        };
                    };
                };
                static_assert(sizeof(MessageHeader) == 26);
                #pragma pack(pop)

                struct IndexedMessageData {
                    std::string sender_unique_id;
                    std::string sender_name;
                    std::string message;
                };

                struct IndexedBlockMessage {
                    uint32_t message_offset;
                    MessageHeader header;

                    std::shared_ptr<IndexedMessageData> message_data;
                };

                struct IndexedBlock {
                    bool index_successful;
                    std::deque<IndexedBlockMessage> message_index;
                    std::mutex message_index_lock;
                };
            }

            namespace db {
                struct MessageBlock {
                    std::chrono::system_clock::time_point begin_timestamp;
                    std::chrono::system_clock::time_point end_timestamp;

                    uint64_t block_offset;

                    union {
                        uint16_t flags;
                        struct {
                            //Attention: Order matters!
                            bool flag__unused_0 : 1;
                            bool flag__unused_1 : 1;
                            bool flag__unused_2 : 1;
                            bool flag__unused_3 : 1;
                            bool flag__unused_4 : 1;
                            bool flag__unused_5 : 1;
                            bool flag__unused_6 : 1;
                            bool flag__unused_7 : 1;
                            bool flag__unused_8 : 1;
                            bool flag__unused_9 : 1;
                            bool flag__unused_10 : 1;
                            bool flag__unused_11 : 1;

                            bool flag_finished : 1;
                            bool flag_finished_later : 1; /* if true the block has been closed because we've a newer block. */

                            bool flag_invalid : 1; /* this block is considered as invalid and will be ignored */
                            bool flag_used : 1;
                        };
                    };

                    std::shared_ptr<fio::BlockHeader> block_header;
                    std::shared_ptr<fio::IndexedBlock> indexed_block;
                };
                static_assert(__BYTE_ORDER == __LITTLE_ENDIAN);
            }

            class ConversationManager;
            class Conversation {
                public:
                    Conversation(const std::shared_ptr<ConversationManager>& /* handle */, ChannelId /* channel id */, std::string  /* file name */);
                    ~Conversation();

                    bool initialize(std::string& error);
                    void finalize();

                    inline ChannelId channel_id() { return this->_channel_id; }
                    /* if for some reason we're not able to open the file then we're in volatile mode */
                    inline bool volatile_only() { return this->_volatile || this->_history_length < 0; }
                    void cleanup_cache();

                    void set_history_length(ssize_t length) { this->_history_length = length; }
                    ssize_t history_length() { return this->_history_length; }

                    inline std::chrono::system_clock::time_point last_message() { return this->_last_message_timestamp; }
                    void register_message(ClientDbId sender_database_id, const std::string& sender_unique_id, const std::string& sender_name, const std::chrono::system_clock::time_point& /* timestamp */, const std::string& message);
                    /* Lookup n messages since end timestamp. Upper time limit is begin timestamp */
                    std::deque<std::shared_ptr<ConversationEntry>> message_history(const std::chrono::system_clock::time_point& /* end timestamp */, size_t /* limit */, const std::chrono::system_clock::time_point& /* begin timestamp */);

                    std::deque<std::shared_ptr<ConversationEntry>> message_history(size_t limit) {
                        return this->message_history(std::chrono::system_clock::now(), limit, std::chrono::system_clock::time_point{});
                    }

                    size_t delete_messages(const std::chrono::system_clock::time_point& /* end timestamp */, size_t /* limit */, const std::chrono::system_clock::time_point& /* begin timestamp */, ClientDbId /* database id */);

                    ts_always_inline void set_ref_self(const std::shared_ptr<Conversation>& pointer) {
                        this->_ref_self = pointer;
                    }
                private:
                    std::weak_ptr<Conversation> _ref_self;
                    std::weak_ptr<ConversationManager> _ref_handle;
                    ts_always_inline std::shared_ptr<ConversationManager> ref_handle() {
                        return this->_ref_handle.lock();
                    }

                    inline bool setup_file();
                    inline ssize_t fread(void* target, size_t length, ssize_t index, bool acquire_handle);
                    inline ssize_t fwrite(void* target, size_t length, ssize_t index, bool extend_file, bool acquire_handle);

                    /* block db functions */
                    void db_save_block(const std::shared_ptr<db::MessageBlock>& /* block */);
                    std::shared_ptr<db::MessageBlock> db_create_block(uint64_t /* block offset */);

                    /* message blocks */
                    std::mutex message_block_lock;
                    /* blocks sorted desc (newest blocks last in list (push_back)) */
                    std::deque<std::shared_ptr<db::MessageBlock>> message_blocks;
                    /* Access last_message_block only within the write queue or while initializing! */
                    std::shared_ptr<db::MessageBlock> last_message_block; /* is registered within message_blocks,but musnt be the last! */
                    bool load_message_block_header(const std::shared_ptr<db::MessageBlock>& /* block */, std::string& /* error */);
                    bool load_message_block_index(const std::shared_ptr<db::MessageBlock>& /* block */, std::string& /* error */);
                    bool load_messages(const std::shared_ptr<db::MessageBlock>& /* block */, size_t /* begin index */, size_t /* end index */, std::string& /* error */);

                    /* message blocks write stuff */
                    std::shared_ptr<db::MessageBlock> create_new_block(std::string& /* error */);
                    void finish_block(const std::shared_ptr<db::MessageBlock>& /* block */, bool write_file);
                    bool write_block_header(const std::shared_ptr<fio::BlockHeader>& /* header */, size_t /* header index */, std::string& /* error */);

                    /* cached messages */
                    std::mutex _last_messages_lock;
                    std::deque<std::shared_ptr<ConversationEntry>> _last_messages;
                    size_t _last_messages_limit = 100; /* cache max 100 messages */

                    /* write handler */
                    std::mutex _write_loop_lock;
                    std::mutex _write_queue_lock;
                    std::deque<std::shared_ptr<ConversationEntry>> _write_queue;
                    std::shared_ptr<event::ProxiedEventEntry<Conversation>> _write_event;
                    void process_write_queue(const std::chrono::system_clock::time_point&);

                    /* basic file stuff */
                    std::string file_name;
                    std::mutex file_handle_lock;
                    std::chrono::system_clock::time_point last_access;
                    FILE* file_handle = nullptr;
                    ChannelId _channel_id;

                    ssize_t _history_length = 0;
                    bool _volatile = false;

                    std::chrono::system_clock::time_point _last_message_timestamp;
            };

            class ConversationManager {
                public:
                    explicit ConversationManager(const std::shared_ptr<VirtualServer>& /* server */);
                    virtual ~ConversationManager();

                    void initialize(const std::shared_ptr<ConversationManager>& _this);
                    void synchronize_channels();
                    void cleanup_cache();

                    bool conversation_exists(ChannelId /* channel */);
                    std::shared_ptr<Conversation> get(ChannelId /* channel */);
                    std::shared_ptr<Conversation> get_or_create(ChannelId /* channel */);
                    void delete_conversation(ChannelId /* channel */);

                    inline const std::deque<std::shared_ptr<Conversation>> conversations() {
                        std::lock_guard lock(this->_conversations_lock);
                        return this->_conversations;
                    }

                    ts_always_inline std::shared_ptr<VirtualServer> ref_server() {
                        return this->_ref_server.lock();
                    }
                private:
                    std::weak_ptr<ConversationManager> _ref_this;
                    std::weak_ptr<VirtualServer> _ref_server;

                    std::mutex _conversations_lock;
                    std::deque<std::shared_ptr<Conversation>> _conversations;

                    std::string file_path;
            };
        }
    }
}