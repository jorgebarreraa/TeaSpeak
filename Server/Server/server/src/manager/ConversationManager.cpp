#include <utility>

#include "./ConversationManager.h"
#include "../InstanceHandler.h"
#include "src/VirtualServer.h"

#include <experimental/filesystem>
#include <log/LogUtils.h>
#include <misc/sassert.h>

/* for the file */
#include <sys/types.h>
#include <unistd.h>

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;
using namespace ts::server::conversation;

namespace fs = std::experimental::filesystem;

/* Using const O3 to improve unreadability */
#if 0
/* Debug */
0x555555c542a2 push   rbp
0x555555c542a3 mov    rbp,rsp
0x555555c542a6 mov    QWORD PTR [rbp-0x28],rdi
0x555555c542aa mov    QWORD PTR [rbp-0x30],rsi
0x555555c542ae mov    QWORD PTR [rbp-0x38],rdx
0x555555c542b2 mov    QWORD PTR [rbp-0x40],rcx
0x555555c542b6 mov    rax,QWORD PTR [rbp-0x40]
0x555555c542ba mov    QWORD PTR [rbp-0x20],rax
0x555555c542be mov    rax,QWORD PTR [rbp-0x38]
0x555555c542c2 mov    QWORD PTR [rbp-0x18],rax
0x555555c542c6 mov    rax,QWORD PTR [rbp-0x28]
0x555555c542ca mov    QWORD PTR [rbp-0x10],rax
0x555555c542ce mov    rax,QWORD PTR [rbp-0x30]
0x555555c542d2 mov    QWORD PTR [rbp-0x8],rax

/* first loop */
0x555555c542d6 cmp    QWORD PTR [rbp-0x18],0x7
0x555555c542db jbe    0x555555c5431e <apply_crypt(void*, void*, unsigned long, unsigned long)+124>
0x555555c542dd mov    rax,QWORD PTR [rbp-0x18]
0x555555c542e1 and    eax,0x7
0x555555c542e4 mov    rdx,QWORD PTR [rbp-0x20]
0x555555c542e8 mov    ecx,eax
0x555555c542ea shl    rdx,cl
0x555555c542ed mov    rax,rdx
0x555555c542f0 xor    rax,QWORD PTR [rbp-0x18]
0x555555c542f4 xor    QWORD PTR [rbp-0x20],rax
0x555555c542f8 mov    rax,QWORD PTR [rbp-0x10]
0x555555c542fc mov    rax,QWORD PTR [rax]
0x555555c542ff xor    rax,QWORD PTR [rbp-0x20]
0x555555c54303 mov    rdx,rax
0x555555c54306 mov    rax,QWORD PTR [rbp-0x8]
0x555555c5430a mov    QWORD PTR [rax],rdx
0x555555c5430d add    QWORD PTR [rbp-0x8],0x8
0x555555c54312 add    QWORD PTR [rbp-0x10],0x8
0x555555c54317 sub    QWORD PTR [rbp-0x18],0x8
0x555555c5431c jmp    0x555555c542d6 /* first loop */

/* Second loop */
0x555555c5431e cmp    QWORD PTR [rbp-0x18],0x0
0x555555c54323 je     0x555555c54364 <apply_crypt(void*, void*, unsigned long, unsigned long)+194>
0x555555c54325 mov    rax,QWORD PTR [rbp-0x18]
0x555555c54329 and    eax,0x7
0x555555c5432c mov    rdx,QWORD PTR [rbp-0x20]
0x555555c54330 mov    ecx,eax
0x555555c54332 shl    rdx,cl
0x555555c54335 mov    rax,rdx
0x555555c54338 xor    rax,QWORD PTR [rbp-0x18]
0x555555c5433c xor    QWORD PTR [rbp-0x20],rax
0x555555c54340 mov    rax,QWORD PTR [rbp-0x10]
0x555555c54344 movzx  edx,BYTE PTR [rax]
0x555555c54347 mov    rax,QWORD PTR [rbp-0x20]
0x555555c5434b xor    edx,eax
0x555555c5434d mov    rax,QWORD PTR [rbp-0x8]
0x555555c54351 mov    BYTE PTR [rax],dl
0x555555c54353 sub    QWORD PTR [rbp-0x18],0x1
0x555555c54358 add    QWORD PTR [rbp-0x10],0x1
0x555555c5435d add    QWORD PTR [rbp-0x8],0x1
0x555555c54362 jmp    0x555555c5431e /* second loop */
0x555555c54364 nop
0x555555c54365 pop    rbp
0x555555c54366 ret


/* O3 */
0x555555c41d8f push   rbp
0x555555c41d90 cmp    rdx,0x7
0x555555c41d94 mov    r8,rcx
0x555555c41d97 mov    rbp,rsp
0x555555c41d9a push   r14
0x555555c41d9c push   rbx
0x555555c41d9d jbe    0x555555c41df8 <apply_crypt(void*, void*, unsigned long, unsigned long)+105>
0x555555c41d9f lea    rbx,[rdx-0x8]
0x555555c41da3 mov    r10,rsi
0x555555c41da6 mov    r9,rdi
0x555555c41da9 mov    rax,rdx
0x555555c41dac mov    r11,rbx
0x555555c41daf and    r11d,0x7
0x555555c41db3 mov    ecx,eax
0x555555c41db5 mov    r14,r8
0x555555c41db8 add    r10,0x8
0x555555c41dbc and    ecx,0x7
0x555555c41dbf add    r9,0x8
0x555555c41dc3 shl    r14,cl
0x555555c41dc6 mov    rcx,r14
0x555555c41dc9 xor    rcx,rax
0x555555c41dcc sub    rax,0x8
0x555555c41dd0 xor    r8,rcx
0x555555c41dd3 mov    rcx,QWORD PTR [r9-0x8]
0x555555c41dd7 xor    rcx,r8
0x555555c41dda mov    QWORD PTR [r10-0x8],rcx
0x555555c41dde cmp    rax,r11
0x555555c41de1 jne    0x555555c41db3 <apply_crypt(void*, void*, unsigned long, unsigned long)+36>
0x555555c41de3 shr    rbx,0x3
0x555555c41de7 and    edx,0x7
0x555555c41dea lea    rax,[rbx*8+0x8]
0x555555c41df2 add    rdi,rax
0x555555c41df5 add    rsi,rax
0x555555c41df8 test   rdx,rdx
0x555555c41dfb je     0x555555c41ed3 <apply_crypt(void*, void*, unsigned long, unsigned long)+324>
0x555555c41e01 mov    rax,r8
0x555555c41e04 mov    ecx,edx
0x555555c41e06 xor    r8,rdx
0x555555c41e09 shl    rax,cl
0x555555c41e0c mov    rcx,rdx
0x555555c41e0f xor    r8,rax
0x555555c41e12 movzx  eax,BYTE PTR [rdi]
0x555555c41e15 xor    eax,r8d
0x555555c41e18 sub    rcx,0x1
0x555555c41e1c mov    BYTE PTR [rsi],al
0x555555c41e1e je     0x555555c41ed3 <apply_crypt(void*, void*, unsigned long, unsigned long)+324>
0x555555c41e24 mov    rax,r8
0x555555c41e27 xor    r8,rcx
0x555555c41e2a shl    rax,cl
0x555555c41e2d mov    rcx,rdx
0x555555c41e30 xor    r8,rax
0x555555c41e33 movzx  eax,BYTE PTR [rdi+0x1]
0x555555c41e37 xor    eax,r8d
0x555555c41e3a sub    rcx,0x2
0x555555c41e3e mov    BYTE PTR [rsi+0x1],al
0x555555c41e41 je     0x555555c41ed3 <apply_crypt(void*, void*, unsigned long, unsigned long)+324>
0x555555c41e47 mov    rax,r8
0x555555c41e4a xor    r8,rcx
0x555555c41e4d shl    rax,cl
0x555555c41e50 mov    rcx,rdx
0x555555c41e53 xor    r8,rax
0x555555c41e56 movzx  eax,BYTE PTR [rdi+0x2]
0x555555c41e5a xor    eax,r8d
0x555555c41e5d sub    rcx,0x3
0x555555c41e61 mov    BYTE PTR [rsi+0x2],al
0x555555c41e64 je     0x555555c41ed3 <apply_crypt(void*, void*, unsigned long, unsigned long)+324>
0x555555c41e66 mov    rax,r8
0x555555c41e69 xor    r8,rcx
0x555555c41e6c shl    rax,cl
0x555555c41e6f mov    rcx,rdx
0x555555c41e72 xor    r8,rax
0x555555c41e75 movzx  eax,BYTE PTR [rdi+0x3]
0x555555c41e79 xor    eax,r8d
0x555555c41e7c sub    rcx,0x4
0x555555c41e80 mov    BYTE PTR [rsi+0x3],al
0x555555c41e83 je     0x555555c41ed3 <apply_crypt(void*, void*, unsigned long, unsigned long)+324>
0x555555c41e85 mov    rax,r8
0x555555c41e88 xor    r8,rcx
0x555555c41e8b shl    rax,cl
0x555555c41e8e mov    rcx,rdx
0x555555c41e91 xor    r8,rax
0x555555c41e94 movzx  eax,BYTE PTR [rdi+0x4]
0x555555c41e98 xor    eax,r8d
0x555555c41e9b sub    rcx,0x5
0x555555c41e9f mov    BYTE PTR [rsi+0x4],al
0x555555c41ea2 je     0x555555c41ed3 <apply_crypt(void*, void*, unsigned long, unsigned long)+324>
0x555555c41ea4 mov    rax,r8
0x555555c41ea7 xor    r8,rcx
0x555555c41eaa shl    rax,cl
0x555555c41ead xor    r8,rax
0x555555c41eb0 movzx  eax,BYTE PTR [rdi+0x5]
0x555555c41eb4 xor    eax,r8d
0x555555c41eb7 cmp    rdx,0x6
0x555555c41ebb mov    BYTE PTR [rsi+0x5],al
0x555555c41ebe je     0x555555c41ed3 <apply_crypt(void*, void*, unsigned long, unsigned long)+324>
0x555555c41ec0 lea    rax,[r8+r8*1]
0x555555c41ec4 xor    r8,0x1
0x555555c41ec8 xor    r8,rax
0x555555c41ecb xor    r8b,BYTE PTR [rdi+0x6]
0x555555c41ecf mov    BYTE PTR [rsi+0x6],r8b
0x555555c41ed3 pop    rbx
0x555555c41ed4 pop    r14
0x555555c41ed6 pop    rbp
0x555555c41ed7 ret
#endif
__attribute__((optimize("-O3"), always_inline)) void apply_crypt(void* source, void* target, size_t length, uint64_t base_key) {
    uint64_t crypt_key{base_key};
    size_t length_left{length};
    auto source_ptr = (uint8_t*) source;
    auto dest_ptr = (uint8_t*) target;

    while(length_left >= 8) {
        crypt_key ^= (crypt_key << (length_left & 0x7U)) ^ length_left;
        *(uint64_t*) dest_ptr = *(uint64_t*) source_ptr ^ crypt_key;

        dest_ptr += 8;
        source_ptr += 8;
        length_left -= 8;
    }
    while(length_left > 0) {
        crypt_key ^= (crypt_key << (length_left & 0x7U)) ^ length_left;
        *dest_ptr = *source_ptr ^ (uint8_t) crypt_key;

        length_left--;
        source_ptr++;
        dest_ptr++;
    }
}
Conversation::Conversation(const std::shared_ptr<ts::server::conversation::ConversationManager> &handle, ts::ChannelId channel_id, std::string file) : _ref_handle(handle), _channel_id(channel_id), file_name(std::move(file)) { }

Conversation::~Conversation() {
    this->finalize();
}

bool Conversation::initialize(std::string& error) {
    auto ref_self = this->_ref_self.lock();
    assert(ref_self);

    auto handle = this->_ref_handle.lock();
    assert(handle);

    auto ref_server = handle->ref_server();
    if(!ref_server) {
        error = "invalid server ref";
        return false;
    }

    auto file = fs::u8path(this->file_name);
    if(!fs::is_directory(file.parent_path())) {
        debugMessage(ref_server->getServerId(), "[Conversations] Creating conversation containing directory {}", file.parent_path().string());
        try {
            fs::create_directories(file.parent_path());
        } catch(fs::filesystem_error& ex) {
            error = "failed to create data directories (" + to_string(ex.code().value()) + "|" + string(ex.what()) + ")";
            return false;
        }
    }

    this->file_handle = fopen(this->file_name.c_str(), fs::exists(file) ? "r+" : "w+");
    if(!this->file_handle) {
        this->_volatile = true;
        error = "failed to open file";
        return false;
    }
    setbuf(this->file_handle, nullptr); /* we're doing random access (a buffer is useless here) */

    auto sql = ref_server->getSql();

    auto result = sql::command(sql, "SELECT `begin_timestamp`, `end_timestamp`, `block_offset`, `flags` FROM `conversation_blocks` WHERE `server_id` = :sid AND `conversation_id` = :cid",
                 variable{":sid", ref_server->getServerId()}, variable{":cid", this->_channel_id}).query([&](int count, std::string* values, std::string* names) {
        std::chrono::system_clock::time_point begin_timestamp{}, end_timestamp{};
        uint64_t block_offset = 0;
        uint16_t flags = 0;

        try {
            for(int index = 0; index < count; index++) {
                if(names[index] == "begin_timestamp")
                    begin_timestamp += milliseconds(stoll(values[index]));
                else if(names[index] == "end_timestamp")
                    end_timestamp += milliseconds(stoll(values[index]));
                else if(names[index] == "block_offset")
                    block_offset = stoull(values[index]);
                else if(names[index] == "flags")
                    flags = (uint16_t) stoll(values[index]);
            }
        } catch(std::exception& ex) {
            logError(ref_server->getServerId(), "[Conversations] Failed to parse conversation block entry! Exception: {}", ex.what());
            return 0;
        }

        auto block = make_shared<db::MessageBlock>(db::MessageBlock{
                begin_timestamp,
                end_timestamp,

                block_offset,
                {flags},

                nullptr,
                nullptr
        });

        /* we dont load invalid blocks */
        if(block->flag_invalid)
            return 0;

        this->message_blocks.push_back(block);
        return 0;
    });
    LOG_SQL_CMD(result);

    /* find duplicates and remove them */
    {
        map<uint64_t, shared_ptr<db::MessageBlock>> blocks;
        for(auto& block : this->message_blocks) {
            auto& entry = blocks[block->block_offset];
            if(entry) {
                debugMessage(ref_server->getServerId(), "[Conversations][{}] Found duplicated block at index {}. Using newest block and dropping old one.", this->_channel_id, block->block_offset);
                if(entry->begin_timestamp < block->begin_timestamp) {
                    entry->flag_invalid = true;
                    this->db_save_block(entry);
                    entry = block;
                } else {
                    block->flag_invalid = true;
                    this->db_save_block(block);
                }
            } else
                entry = block;
        }

        /* lets remove the invalid blocks */
        this->message_blocks.erase(std::find_if(this->message_blocks.begin(), this->message_blocks.end(), [](const shared_ptr<db::MessageBlock>& block) { return block->flag_invalid; }), this->message_blocks.end());
    }

    /* lets find the last block */
    if(!this->message_blocks.empty()) {
        debugMessage(ref_server->getServerId(), "[Conversations][{}] Loaded {} blocks. Trying to find last block.", this->_channel_id, this->message_blocks.size());
        deque<shared_ptr<db::MessageBlock>> open_blocks;
        for(auto& block : this->message_blocks)
            if(!block->flag_finished)
                open_blocks.push_back(block);

        logTrace(ref_server->getServerId(), "[Conversations][{}] Found {} unfinished blocks. Searching for the \"latest\" and closing all previous blocks.", this->_channel_id, open_blocks.size());
        shared_ptr<db::MessageBlock> latest_block;
        const auto calculate_latest_block = [&](bool open_only) {
            latest_block = nullptr;
            for(auto& block : open_blocks) {
                if(block->flag_invalid)
                    continue;

                if(!latest_block || latest_block->begin_timestamp < block->begin_timestamp) {
                    if(latest_block) {
                        latest_block->flag_finished_later = true;
                        latest_block->flag_finished = true;
                        this->db_save_block(latest_block);
                    }

                    latest_block = block;
                }
            }
        };
        calculate_latest_block(true);

        if(latest_block) {
            logTrace(ref_server->getServerId(), "[Conversations][{}] Found a latest block at index {}. Verify block with file.", this->_channel_id, latest_block->block_offset);

            const auto verify_block = [&] {
                auto result = fseek(this->file_handle, 0, SEEK_END);
                if(result != 0) {
                    error = "failed to seek to the end (" + to_string(result) + " | " + to_string(errno) + ")";
                    return;
                }

                auto file_size = ftell(this->file_handle);
                if(file_size < 0) {
                    error = "failed to tell the end position (" + to_string(file_size) + " | " + to_string(errno) + ")";
                    return;
                }
                logTrace(ref_server->getServerId(), "[Conversations][{}] File total size {}, last block index {}", this->_channel_id, file_size, latest_block->block_offset);
                if(file_size < (latest_block->block_offset + sizeof(fio::BlockHeader))) {
                    latest_block->flag_finished_later = true;
                    latest_block->flag_invalid = true;
                    this->finish_block(latest_block, false);

                    logTrace(ref_server->getServerId(), "[Conversations][{}] File total size is less than block requires. Appending a new block at the end of the file.", this->_channel_id, latest_block->block_offset);
                    latest_block = nullptr;
                    return;
                }

                if(!this->load_message_block_header(latest_block, error)) {
                    latest_block->flag_finished_later = true;
                    latest_block->flag_invalid = true;
                    this->finish_block(latest_block, false);

                    logTrace(ref_server->getServerId(), "[Conversations][{}] Failed to load latest block at file index {}: {}. Appending an new one.", this->_channel_id, latest_block->block_offset, error);
                    error = "";
                    latest_block = nullptr;
                    return;
                }

                /* We've a valid last block. Now the general write function could decide if we want a new block. */
                this->last_message_block = latest_block;
                logTrace(ref_server->getServerId(), "[Conversations][{}] Last db saved block valid. Reusing it.", this->_channel_id, latest_block->block_offset, error);
            };
            verify_block();
            if(!error.empty()) {
                latest_block->flag_finished_later = true;
                latest_block->flag_invalid = true;
                this->finish_block(latest_block, false);

                logError(ref_server->getServerId(), "[Conversations][{}] Could not verify last block. Appending a new one at the end of the file.", this->_channel_id);
                latest_block = nullptr;
            }
            error = "";
        } else {
            logTrace(ref_server->getServerId(), "[Conversations][{}] Found no open last block. Using a new one.", this->_channel_id);
        }
    } else {
        debugMessage(ref_server->getServerId(), "[Conversations][{}] Found no blocks. Creating new at the end of the file.", this->_channel_id, this->message_blocks.size());
    }

    std::stable_sort(this->message_blocks.begin(), this->message_blocks.end(), [](const shared_ptr<db::MessageBlock>& a, const shared_ptr<db::MessageBlock>& b) { return a->begin_timestamp < b->begin_timestamp; });
    this->_write_event = make_shared<event::ProxiedEventEntry<Conversation>>(ts::event::ProxiedEventEntry<Conversation>(ref_self, &Conversation::process_write_queue));

    /* set the last message timestamp property */
    {
        auto last_message = this->message_history(1);
        if(!last_message.empty())
            this->_last_message_timestamp = last_message.back()->message_timestamp;
        else
            this->_last_message_timestamp = system_clock::time_point{};
    }
    /* close the file handle because we've passed our checks */
    {
        fclose(this->file_handle);
        this->file_handle = nullptr;
    }
    return true;
}

void Conversation::finalize() {
    this->_write_event = nullptr; /* we dont want to schedule/execute new events! */
    this->_write_loop_lock.lock(); /* wait until current write proceed */
    this->_write_loop_lock.unlock();

    //TODO: May flush?

    {
        lock_guard lock(this->message_block_lock);
        this->message_blocks.clear();
    }
}

void Conversation::cleanup_cache() {
    auto ref_handle = this->ref_handle();
    if(!ref_handle)
        return;
    auto ref_server = ref_handle->ref_server();
    if(!ref_server)
        return;

    {
        lock_guard block(this->message_block_lock);
        for(auto& block : this->message_blocks) {
            block->block_header = nullptr;
            block->indexed_block = nullptr;
        }
    }
    {
        lock_guard file_lock(this->file_handle_lock);
        if(this->last_access + minutes(5) < system_clock::now()) {
            if(this->file_handle) {
                fclose(this->file_handle);
                this->file_handle = nullptr;
                debugMessage(ref_server->getServerId(), "[Conversations][{}] Closing file handle due to inactivity.", this->_channel_id);
            }
        }
    }
}

bool Conversation::setup_file() {
    this->file_handle = fopen(this->file_name.c_str(), fs::exists(this->file_name) ? "r+" : "w+");
    if(!this->file_handle) {
        auto ref_handle = this->ref_handle();
        if(!ref_handle)
            return false;
        auto ref_server = ref_handle->ref_server();
        if(!ref_server)
            return false;

        logError(ref_server->getServerId(), "[Conversations][{}] Failed to open closed file handle. ({} | {})", errno, strerror(errno));
        return false;
    }
    setbuf(this->file_handle, nullptr); /* we're doing random access (a buffer is useless here) */
    return true;
}

ssize_t Conversation::fread(void *target, size_t length, ssize_t index, bool acquire_lock) {
    if(length == 0)
        return 0;

    unique_lock file_lock(this->file_handle_lock, defer_lock);
    if(acquire_lock)
        file_lock.lock();
    this->last_access = system_clock::now();
    if(!this->file_handle && !this->setup_file())
        return -3;
    if(index >= 0) {
        auto result = fseek(this->file_handle, index, SEEK_SET);
        if(result < 0)
            return -2;
    }

    size_t total_read = 0;
    while(total_read < length) {
        auto read = ::fread_unlocked((char*) target + total_read, 1, length - total_read, this->file_handle);
        if(read <= 0)
            return -1;
        total_read += read;
    }
    return total_read;
}

ssize_t Conversation::fwrite(void *target, size_t length, ssize_t index, bool extend_file, bool acquire_lock) {
    if(length == 0)
        return 0;

    unique_lock file_lock(this->file_handle_lock, defer_lock);
    if(acquire_lock)
        file_lock.lock();
    extend_file = false; /* fseek does the job good ad well */
    if(!this->file_handle && !this->setup_file())
        return -3;
    this->last_access = system_clock::now();
    if(index >= 0) {
        auto result = extend_file ? lseek(fileno(this->file_handle), index, SEEK_SET) : fseek(this->file_handle, index, SEEK_SET);
        if(result < 0)
            return -2;
    }

    size_t total_written = 0;
    while(total_written < length) {
        auto written = ::fwrite_unlocked((char*) target + total_written, 1, length - total_written, this->file_handle);
        if(written <= 0)
            return -1;
        total_written += written;
    }
    return total_written;
}

bool Conversation::load_message_block_header(const std::shared_ptr<ts::server::conversation::db::MessageBlock> &block, std::string &error) {
    if(block->block_header)
        return true;

    auto block_header = make_unique<fio::BlockHeader>();
    auto read = this->fread(&*block_header, sizeof(*block_header), block->block_offset, true);
    if(read != sizeof(*block_header)) {
        error = "failed to read block header (read " + to_string(read) + " out of " + to_string(sizeof(*block_header)) + ": " + to_string(errno) + ")";
        return false;
    }
    if(block_header->version != 1) {
        error = "block version missmatch (block version: " + to_string(block_header->version) + ", current version: 1)";
        return false;
    }

    if(block_header->cookie != fio::BlockHeader::HEADER_COOKIE) {
        error = "block cookie missmatch";
        return false;
    }

    block->block_header = move(block_header);
    return true;
}

bool Conversation::load_message_block_index(const std::shared_ptr<ts::server::conversation::db::MessageBlock> &block, std::string& error) {
    if(block->indexed_block)
        return true;

    auto index = make_shared<fio::IndexedBlock>();
    index->index_successful = false;
    {
        if(!this->load_message_block_header(block, error)) {
            error = "failed to load block header: " + error;
            return false;
        }
        auto block_header = block->block_header;
        if(!block_header) {
            error = "failed to reference block header ";
            return false;
        }

        if(block_header->block_size > fio::BlockHeader::MAX_BLOCK_SIZE) {
            error = "block contains too many messages (" + to_string(block_header->block_size) + ")";
            return false;
        }

        size_t offset = block->block_offset;
        offset += sizeof(fio::BlockHeader);
        size_t max_offset = block->block_offset + block_header->block_size; /* block_size := Written size, must be smaller or equal to the max size, except max size is 0 */

        fio::MessageHeader header{};
        while(offset < max_offset) {
            if(this->fread(&header, sizeof(header), offset, true) != sizeof(header)) {
                error = "failed to read message header at index" + to_string(offset);
                return false;
            }
            if(header.cookie != fio::MessageHeader::HEADER_COOKIE) {
                error = "failed to verify message header cookie at index " + to_string(offset);
                return false;
            }
            index->message_index.emplace_back(fio::IndexedBlockMessage{(uint32_t) (offset - block->block_offset), {header}, nullptr});
            offset += header.total_length;
        }
    }
    block->indexed_block = index;
    return true;
}

bool Conversation::load_messages(const std::shared_ptr<db::MessageBlock> &block, size_t index, size_t end_index, std::string &error) {
    if(!this->load_message_block_index(block, error)) {
        error = "failed to index block: " + error;
        return false;
    }

    auto indexed_block = block->indexed_block;
    auto header = block->block_header;
    if(!indexed_block || !header) {
        error = "failed to reference required data";
        return false;
    }

    /* Note: We dont lock message_index_lock here because the write thread only increases the list and dont take stuff away where we could pointing at! */
    if(index >= indexed_block->message_index.size())
        return true;


    unique_lock file_lock(this->file_handle_lock);
    if(!this->file_handle && !this->setup_file()) {
        error = "failed to open file handle";
        return false;
    }
    auto result = fseek(this->file_handle, block->block_offset + indexed_block->message_index[index].message_offset, SEEK_SET);
    if(result == EINVAL) {
        error = "failed to seek to begin of an indexed block read";
        return false;
    }

    /*
     * We dont need to lock the message_index_lock here because we never delete the messages and we just iterate with index
     */
    while(index < end_index && index < indexed_block->message_index.size()) {
        auto& message_data = indexed_block->message_index[index];
        if(message_data.message_data) {
            index++;
            continue;
        }

        auto result = fseek(this->file_handle, sizeof(fio::MessageHeader), SEEK_CUR);
        if(result == EINVAL) {
            error = "failed to seek to begin of the message data";
            return false;
        }

        auto data = make_shared<fio::IndexedMessageData>();
        if(header->meta_encrypted) {
            auto meta_size = message_data.header.sender_unique_id_length + message_data.header.sender_name_length;
            auto meta_buffer = malloc(meta_size);

            if(this->fread(meta_buffer, meta_size, -1, false) != meta_size) {
                error = "failed to read message metadata at " + to_string(index);
                free(meta_buffer);
                return false;
            }

            apply_crypt(meta_buffer, meta_buffer, meta_size, (block->block_offset ^ message_data.header.message_timestamp) ^ 0x6675636b20796f75ULL); /* 0x6675636b20796f75 := 'fuck you' */

            data->sender_unique_id.assign((char*) meta_buffer, message_data.header.sender_unique_id_length);
            data->sender_name.assign((char*) meta_buffer + message_data.header.sender_unique_id_length, message_data.header.sender_name_length);
            free(meta_buffer);
        } else {
            data->sender_unique_id.resize(message_data.header.sender_unique_id_length);
            data->sender_name.resize(message_data.header.sender_name_length);

            if(this->fread(data->sender_unique_id.data(), data->sender_unique_id.length(), -1, false) != data->sender_unique_id.length()) {
                error = "failed to read message sender unique id at " + to_string(index);
                return false;
            }

            if(this->fread(data->sender_name.data(), data->sender_name.length(), -1, false) != data->sender_name.length()) {
                error = "failed to read message sender name id at " + to_string(index);
                return false;
            }
        }

        data->message.resize(message_data.header.message_length);
        if(this->fread(data->message.data(), data->message.length(), -1, false) != data->message.length()) {
            error = "failed to read message id at " + to_string(index);
            return false;
        }

        if(header->message_encrypted)
            apply_crypt(data->message.data(), data->message.data(), data->message.size(), block->block_offset ^ message_data.header.message_timestamp);

        message_data.message_data = data;
        index++;
    }

    return true;
}

void Conversation::finish_block(const std::shared_ptr<ts::server::conversation::db::MessageBlock> &block, bool write_file) {
    if(block->flag_finished)
        return;

    auto handle = this->_ref_handle.lock();
    sassert(handle);
    if(!handle)
        return;

    auto ref_server = handle->ref_server();
    if(!ref_server)
        return;

    block->flag_finished = true;

    if(write_file) {
        string error;
        auto result = this->load_message_block_header(block, error);
        auto header = block->block_header;
        result &= !!header; /* only success if we really have a header */
        if(result) {
            if(header->block_max_size == 0) {
                header->block_max_size = header->block_size;
                header->finished = true;
                if(!this->write_block_header(header, block->block_offset, error)) {
                    logError(ref_server->getServerId(), "Failed to finish block because block header could not be written: {}", error);
                    block->flag_invalid = true; /* because we cant set the block size we've to declare that block as invalid */
                }
            }
        } else {
            logError(ref_server->getServerId(), "Failed to finish block because block header could not be set: {}", error);
            block->flag_invalid = true; /* because we cant set the block size we've to declare that block as invalid */
        }
    } else {
        block->flag_invalid = true; /* because we dont write the block we cant ensure a valid block */
    }

    this->db_save_block(block);
}

bool Conversation::write_block_header(const std::shared_ptr<fio::BlockHeader> &header, size_t index, std::string &error) {
    auto code = this->fwrite(&*header, sizeof(fio::BlockHeader), index, false, true);
    if(code == sizeof(fio::BlockHeader))
        return true;
    error = "write returned " + to_string(code);
    return false;
}

void Conversation::process_write_queue(const std::chrono::system_clock::time_point &scheduled_time) {
    unique_lock write_lock(this->_write_loop_lock, try_to_lock);
    if(!write_lock.owns_lock()) /* we're already writing if this lock fails */
        return;

    unique_lock write_queue_lock(this->_write_queue_lock, defer_lock);
    std::shared_ptr<ConversationEntry> write_entry;
    fio::MessageHeader write_header{};
    std::shared_ptr<fio::BlockHeader> block_header;
    auto handle = this->_ref_handle.lock();
    sassert(handle);
    if(!handle)
        return;

    auto ref_server = handle->ref_server();
    if(!ref_server)
        return;

    write_header.cookie = fio::MessageHeader::HEADER_COOKIE;
    write_header.message_flags = 0;

    while(true) {
        write_queue_lock.lock();
        if(this->_write_queue.empty())
            break;
        write_entry = this->_write_queue.front();
        this->_write_queue.pop_front();
        write_queue_lock.unlock();

        /* calculate the write message total length */
        write_header.message_length =  (uint16_t) min(write_entry->message.size(), (size_t) (65536 - 1));
        write_header.sender_name_length = (uint8_t) min(write_entry->sender_name.size(), (size_t) 255);
        write_header.sender_unique_id_length = (uint8_t) min(write_entry->sender_unique_id.size(), (size_t) 255);
        write_header.total_length = sizeof(write_header) + write_header.sender_unique_id_length + write_header.sender_name_length + write_header.message_length;

        /* verify last block */
        if(this->last_message_block) {
            block_header = this->last_message_block->block_header;

            if(!block_header) {
                logError(ref_server->getServerId(), "[Conversations][{}] Current last block contains no header! Try to finish it and creating a new one.", this->_channel_id);
                this->finish_block(this->last_message_block, true);
                this->last_message_block = nullptr;
            } else if(this->last_message_block->flag_finished)
                this->last_message_block = nullptr;
            else {
                if(this->last_message_block->begin_timestamp + hours(24) < scheduled_time) {
                    debugMessage(ref_server->getServerId(), "[Conversations][{}] Beginning new block. Old block is older than 24hrs. ({})", this->_channel_id, this->last_message_block->begin_timestamp.time_since_epoch().count());
                    this->finish_block(this->last_message_block, true);
                    this->last_message_block = nullptr;
                } else if((block_header->block_max_size != 0 && block_header->block_size + write_header.total_length >= block_header->block_max_size) || block_header->block_size > fio::BlockHeader::MAX_BLOCK_SIZE){
                    debugMessage(ref_server->getServerId(), "[Conversations][{}] Beginning new block. Old block would exceed his space (Current index: {}, Max index: {}, Soft cap: {}, Message size: {}).",
                                 this->_channel_id, block_header->block_size, block_header->block_max_size, fio::BlockHeader::MAX_BLOCK_SIZE, write_header.total_length);
                    this->finish_block(this->last_message_block, true);
                    this->last_message_block = nullptr;
                } else if(block_header->message_version != 1){
                    debugMessage(ref_server->getServerId(), "[Conversations][{}] Beginning new block. Old block had another message version (Current version: {}, Block version: {}).", this->_channel_id, 1, block_header->message_version);
                    this->finish_block(this->last_message_block, true);
                    this->last_message_block = nullptr;
                }
            }
        }

        /* test if we have a block or create a new one at the begin of the file */
        if(!this->last_message_block) {
            //Note: If we reuse blocks we've to reorder them within message_blocks (newest blocks are at the end)
            //TODO: Find "free" blocks and use them! (But do not use indirectly finished blocks, their max size could be invalid)

            unique_lock file_lock(this->file_handle_lock);
            if(!this->file_handle && !this->setup_file()) {
                logError(ref_server->getServerId(), "[Conversations][{}] Failed to reopen log file. Dropping message!", this->_channel_id);
                return;
            }
            auto result = fseek(this->file_handle, 0, SEEK_END);
            if(result != 0) {
                logError(ref_server->getServerId(), "[Conversations][{}] failed to seek to the end (" + to_string(result) + " " + to_string(errno) + "). Could not create new block. Dropping message!", this->_channel_id);
                return;
            }

            auto file_size = ftell(this->file_handle);
            if(file_size < 0) {
                logError(ref_server->getServerId(), "[Conversations][{}] failed to tell the end position (" + to_string(result) + " " + to_string(errno) + "). Could not create new block. Dropping message!", this->_channel_id);
                return;
            }
            file_lock.unlock();
            this->last_message_block = this->db_create_block((uint64_t) file_size);
            if(!this->last_message_block) {
                logError(ref_server->getServerId(), "[Conversations][{}] Failed to create a new block within database. Dropping message!", this->_channel_id);
                return;
            }
            block_header = make_shared<fio::BlockHeader>();
            memset(&*block_header, 0, sizeof(fio::BlockHeader));

            block_header->version = 1;
            block_header->message_version = 1;
            block_header->cookie = fio::BlockHeader::HEADER_COOKIE;
            block_header->first_message_timestamp = (uint64_t) duration_cast<milliseconds>(write_entry->message_timestamp.time_since_epoch()).count();
            block_header->block_size = sizeof(fio::BlockHeader);

            block_header->message_encrypted = true; /* May add some kind of hidden debug option? */
            block_header->meta_encrypted = true; /* May add some kind of hidden debug option? */
            this->last_message_block->block_header = block_header;
        }

        auto entry_offset = this->last_message_block->block_offset + sizeof(fio::BlockHeader) + block_header->last_message_offset;
        write_header.sender_database_id = write_entry->sender_database_id;
        write_header.message_timestamp = (uint64_t) duration_cast<milliseconds>(write_entry->message_timestamp.time_since_epoch()).count();
        block_header->last_message_timestamp = write_header.message_timestamp;

        /* first write the header */
        if(this->fwrite(&write_header, sizeof(write_header), entry_offset, true, true) != sizeof(write_header)) {
            logError(ref_server->getServerId(), "[Conversations][{}] Failed to write message header. Dropping message!", this->_channel_id);
            return;
        }
        entry_offset += sizeof(write_header);

        /* write the metadata */
        {
            auto write_buffer_size = write_header.sender_unique_id_length + write_header.sender_name_length;
            auto write_buffer = malloc(write_buffer_size);

            memcpy(write_buffer, write_entry->sender_unique_id.data(), write_header.sender_unique_id_length);
            memcpy((char*) write_buffer + write_header.sender_unique_id_length, write_entry->sender_name.data(), write_header.sender_name_length);

            if(block_header->meta_encrypted)
                apply_crypt(write_buffer, write_buffer, write_buffer_size, (this->last_message_block->block_offset ^ write_header.message_timestamp) ^ 0x6675636b20796f75ULL); /* 0x6675636b20796f75 := 'fuck you' */

            /* then write the sender unique id */
            if(this->fwrite(write_buffer, write_buffer_size, entry_offset, true, true) != write_buffer_size) {
                logError(ref_server->getServerId(), "[Conversations][{}] Failed to write message header. Dropping message!", this->_channel_id);
                free(write_buffer);
                return;
            }
            free(write_buffer);
            entry_offset += write_buffer_size;
        }

        /* then write the message */
        {
            bool message_result;
            if(block_header->message_encrypted) {
                size_t length = write_entry->message.size();
                char* target_buffer = (char*) malloc(length);
                apply_crypt(write_entry->message.data(), target_buffer, length, this->last_message_block->block_offset ^ write_header.message_timestamp);

                message_result = this->fwrite(target_buffer, write_header.message_length, entry_offset, true, true) == write_header.message_length;
                free(target_buffer);
            } else {
                message_result = this->fwrite(write_entry->message.data(), write_header.message_length, entry_offset, true, true) == write_header.message_length;
            }
            if(!message_result) {
                logError(ref_server->getServerId(), "[Conversations][{}] Failed to write message. Dropping message!", this->_channel_id);
                return;
            }
            entry_offset += write_header.message_length;
        }

        block_header->last_message_offset = (uint32_t) (entry_offset - this->last_message_block->block_offset - sizeof(fio::BlockHeader));
        block_header->block_size += write_header.total_length;
        block_header->message_count += 1;

        auto indexed_block = this->last_message_block->indexed_block;
        if(indexed_block) {
            lock_guard lock(indexed_block->message_index_lock);
            indexed_block->message_index.push_back(fio::IndexedBlockMessage{(uint32_t) (entry_offset - this->last_message_block->block_offset), {write_header}, nullptr});
        }
    }

    if(write_header.total_length != 0) {/* will be set when at least one message has been written */
        this->db_save_block(this->last_message_block);

        string error;
        if(!this->write_block_header(block_header, this->last_message_block->block_offset, error)) {
            logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to write block header after message write ({}). This could cause data loss!", this->_channel_id, error);
        }
    }
}

std::shared_ptr<db::MessageBlock> Conversation::db_create_block(uint64_t offset) {
    auto result = make_shared<db::MessageBlock>();
    result->block_offset = offset;
    result->begin_timestamp = system_clock::now();
    result->end_timestamp = system_clock::now();
    result->flags = 0;
    result->flag_used = true;
    result->flag_invalid = true; /* first set it to invalid for the database so it becomes active as soon somebody uses it */

    auto handle = this->_ref_handle.lock();
    assert(handle);

    auto ref_server = handle->ref_server();
    if(!ref_server)
        return nullptr;

    auto sql = ref_server->getSql();
    if(!sql)
        return nullptr;

    //`server_id` INT, `conversation_id` INT, `begin_timestamp` INT, `end_timestamp` INT, `block_offset` INT, `flags` INT
    auto sql_result = sql::command(sql, "INSERT INTO `conversation_blocks` (`server_id`, `conversation_id`, `begin_timestamp`, `end_timestamp`, `block_offset`, `flags`) VALUES (:sid, :cid, :bt, :et, :bo, :f);",
            variable{":sid", ref_server->getServerId()},
            variable{":cid", this->_channel_id},
            variable{":bt", duration_cast<milliseconds>(result->begin_timestamp.time_since_epoch()).count()},
            variable{":et", duration_cast<milliseconds>(result->end_timestamp.time_since_epoch()).count()},
            variable{":bo", offset},
            variable{":f", result->flags}
    ).executeLater();
    sql_result.waitAndGetLater(LOG_SQL_CMD, {-1, "future error"});

    {
        lock_guard lock(this->message_block_lock);
        this->message_blocks.push_back(result);
    }

    result->flag_invalid = false;
    return result;
}

void Conversation::db_save_block(const std::shared_ptr<db::MessageBlock> &block) {
    auto handle = this->_ref_handle.lock();
    assert(handle);

    auto ref_server = handle->ref_server();
    if(!ref_server) {
        logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to update block db info (server expired)", this->_channel_id);
        return;
    }

    auto sql = ref_server->getSql();
    if(!sql) {
        logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to update block db info (sql expired)", this->_channel_id);
        return;
    }

    auto sql_result = sql::command(sql, "UPDATE `conversation_blocks` SET `end_timestamp` = :et, `flags` = :f WHERE `server_id` = :sid AND `conversation_id` = :cid AND `begin_timestamp` = :bt AND `block_offset` = :bo",
                                   variable{":sid", ref_server->getServerId()},
                                   variable{":cid", this->_channel_id},
                                   variable{":bt", duration_cast<milliseconds>(block->begin_timestamp.time_since_epoch()).count()},
                                   variable{":et", duration_cast<milliseconds>(block->end_timestamp.time_since_epoch()).count()},
                                   variable{":bo", block->block_offset},
                                   variable{":f", block->flags}
    ).executeLater();
    sql_result.waitAndGetLater(LOG_SQL_CMD, {-1, "future error"});
}

void Conversation::register_message(ts::ClientDbId sender_database_id, const std::string &sender_unique_id, const std::string &sender_name, const std::chrono::system_clock::time_point &ts, const std::string &message) {
    auto entry = make_shared<ConversationEntry>();
    entry->message_timestamp = ts;
    this->_last_message_timestamp = entry->message_timestamp;
    entry->message = message;
    entry->sender_name = sender_name;
    entry->sender_unique_id = sender_unique_id;
    entry->sender_database_id = sender_database_id;

    {
        lock_guard lock(this->_last_messages_lock);
        this->_last_messages.push_back(entry);
        while(this->_last_messages.size() > this->_last_messages_limit) {
            this->_last_messages.pop_front(); /* TODO: Use a iterator for delete to improve performance */
        }
    }

    if(!this->volatile_only()) {
        {
            lock_guard lock(this->_write_queue_lock);
            this->_write_queue.push_back(entry);
        }
        auto executor = serverInstance->getConversationIo();
        executor->schedule(this->_write_event);
    }
}

std::deque<std::shared_ptr<ConversationEntry>> Conversation::message_history(const std::chrono::system_clock::time_point &end_timestamp, size_t message_count, const std::chrono::system_clock::time_point &begin_timestamp) {
    std::deque<std::shared_ptr<ConversationEntry>> result;
    if(message_count == 0)
        return result;

    bool count_deleted = false;
    /* first try to fillout the result with the cached messages */
    {
        lock_guard lock(this->_last_messages_lock);
        //TODO: May just insert the rest of the iterator instead of looping?
        for(auto it = this->_last_messages.rbegin(); it != this->_last_messages.rend(); it++) {
            if((*it)->message_timestamp > end_timestamp) /* message has been send after the search timestamp */
                continue;
            if(begin_timestamp.time_since_epoch().count() != 0 && (*it)->message_timestamp < begin_timestamp)
                return result;
            if((*it)->flag_message_deleted && !count_deleted)
                continue;

            result.push_back(*it);
            if(--message_count == 0)
                return result;
        }
    }

    if(!this->volatile_only()) {
        auto handle = this->_ref_handle.lock();
        if(!handle)
            return result;

        auto ref_server = handle->ref_server();
        if(!ref_server)
            return result;

        auto begin_timestamp_ms = chrono::floor<milliseconds>(begin_timestamp.time_since_epoch()).count();
        auto timestamp_ms = result.empty() ? chrono::floor<milliseconds>(end_timestamp.time_since_epoch()).count() : chrono::floor<milliseconds>(result.back()->message_timestamp.time_since_epoch()).count();

        unique_lock lock(this->message_block_lock);
        auto rit = this->message_blocks.end();
        if(rit != this->message_blocks.begin()) {
            bool found = false;
            do {
                rit--;
                if(chrono::floor<milliseconds>((*rit)->begin_timestamp.time_since_epoch()).count() < timestamp_ms) {
                    found = true;
                    break; /* we found the first block which is created before the point we're searching from */
                }
            } while(rit != this->message_blocks.begin());


            string error;
            if(found) {
                vector<shared_ptr<db::MessageBlock>> relevant_entries{this->message_blocks.begin(), ++rit};
                lock.unlock();

                auto _rit = --relevant_entries.end();
                do {
                    auto block = *_rit;
                    /* lets search for messages */
                    if(!this->load_message_block_index(block, error)) {
                        logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to load message block {} for message lookup: {}", this->_channel_id, block->block_offset, error);
                        continue;
                    }
                    auto index = (*_rit)->indexed_block;
                    if(!index) {
                        logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to reference indexed block within message block.", this->_channel_id);
                        continue;
                    }

                    lock_guard index_lock{index->message_index_lock};
                    auto rmid = index->message_index.end();
                    if(rmid == index->message_index.begin())
                        continue; /* Empty block? Funny */

                    auto block_found = false;
                    do {
                        rmid--;
                        if(rmid->header.message_timestamp < timestamp_ms) {
                            block_found = true;
                            break; /* we found the first block which is created before the point we're searching from */
                        }
                    } while(rmid != index->message_index.begin());
                    if(!block_found)
                        continue;


                    if(!this->load_messages(block, 0, std::distance(index->message_index.begin(), rmid) + 1, error)) {
                        logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to load messages within block {} for message lookup: {}", this->_channel_id, block->block_offset, error);
                        continue;
                    }
                    do {
                        if(rmid->header.flag_deleted && !count_deleted)
                            continue;

                        if(rmid->header.message_timestamp >= timestamp_ms)
                            continue; /* for some reason we got a message from the index of before where we are. This could happen for "orphaned" blocks which point to a valid block within the future block  */

                        if(begin_timestamp.time_since_epoch().count() != 0 && rmid->header.message_timestamp < begin_timestamp_ms)
                            return result;

                        auto data = rmid->message_data;
                        if(!data)
                            continue;

                        result.push_back(make_shared<ConversationEntry>(ConversationEntry{
                                system_clock::time_point{} + milliseconds{rmid->header.message_timestamp},

                                (ClientDbId) rmid->header.sender_database_id,
                                data->sender_unique_id,
                                data->sender_name,

                                data->message,

                                rmid->header.flag_deleted
                        }));
                        timestamp_ms = rmid->header.message_timestamp;
                        if(--message_count == 0)
                            return result;
                    } while(rmid-- != index->message_index.begin());
                } while(_rit-- != relevant_entries.begin());
            }
        }
    }

    return result;
}

//TODO: May move the IO write part to the write queue?
size_t Conversation::delete_messages(const std::chrono::system_clock::time_point &end_timestamp, size_t message_count, const std::chrono::system_clock::time_point &begin_timestamp, ts::ClientDbId cldbid) {
    size_t delete_count_volatile = 0, delete_count = 0;

    if(message_count == 0)
        return 0;

    /* first try to fillout the result with the cached messages */
    {
        lock_guard lock(this->_last_messages_lock);
        for(auto it = this->_last_messages.rbegin(); it != this->_last_messages.rend(); it++) {
            if((*it)->message_timestamp > end_timestamp) /* message has been send after the search timestamp */
                continue;

            if(begin_timestamp.time_since_epoch().count() != 0 && (*it)->message_timestamp < begin_timestamp)
                break;

            if(cldbid != 0 && (*it)->sender_database_id != cldbid)
                continue;

            (*it)->flag_message_deleted = true;
            if(++delete_count_volatile >= message_count)
                break;
        }
    }

    /* TODO: Remove from write queue */

    if(!this->volatile_only()) {
        auto handle = this->_ref_handle.lock();
        if(!handle)
            return delete_count_volatile;

        auto ref_server = handle->ref_server();
        if(!ref_server)
            return delete_count_volatile;

        auto begin_timestamp_ms = chrono::floor<milliseconds>(begin_timestamp.time_since_epoch()).count();
        auto timestamp_ms = chrono::floor<milliseconds>(end_timestamp.time_since_epoch()).count();

        unique_lock lock(this->message_block_lock);
        auto rit = this->message_blocks.end();
        if(rit != this->message_blocks.begin()) {
            bool found = false;
            do {
                rit--;
                if(chrono::floor<milliseconds>((*rit)->begin_timestamp.time_since_epoch()).count() < timestamp_ms) {
                    found = true;
                    break; /* we found the first block which is created before the point we're searching from */
                }
            } while(rit != this->message_blocks.begin());


            string error;
            if(found) {
                vector<shared_ptr<db::MessageBlock>> relevant_entries{this->message_blocks.begin(), ++rit};
                lock.unlock();

                auto _rit = --relevant_entries.end();
                do {
                    auto block = *_rit;
                    /* lets search for messages */
                    if(!this->load_message_block_index(block, error)) {
                        logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to load message block {} for message delete: {}", this->_channel_id, block->block_offset, error);
                        continue;
                    }
                    auto index = (*_rit)->indexed_block;
                    if(!index) {
                        logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to reference indexed block within message block.", this->_channel_id);
                        continue;
                    }

                    lock_guard index_lock{index->message_index_lock};
                    auto rmid = index->message_index.end();
                    if(rmid == index->message_index.begin())
                        continue; /* Empty block? Funny */

                    auto block_found = false;
                    do {
                        rmid--;
                        if(rmid->header.message_timestamp < timestamp_ms) {
                            block_found = true;
                            break; /* we found the first block which is created before the point we're searching from */
                        }
                    } while(rmid != index->message_index.begin());
                    if(!block_found)
                        continue;

                    do {
                        if(rmid->header.message_timestamp >= timestamp_ms)
                            continue; /* for some reason we got a message from the index of before where we are. This could happen for "orphaned" blocks which point to a valid block within the future block  */

                        if(begin_timestamp.time_since_epoch().count() != 0 && rmid->header.message_timestamp < begin_timestamp_ms)
                            return max(delete_count, delete_count_volatile);

                        if(cldbid != 0 && rmid->header.sender_database_id != cldbid)
                            continue;

                        if(!rmid->header.flag_deleted) {
                            rmid->header.flag_deleted = true;

                            auto offset = block->block_offset + rmid->message_offset;
                            if(this->fwrite(&rmid->header, sizeof(rmid->header), offset, false, true) != sizeof(rmid->header))
                                logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to save message flags.", this->_channel_id);
                        }

                        timestamp_ms = rmid->header.message_timestamp;
                        if(++delete_count >= message_count)
                            return max(delete_count, delete_count_volatile);
                    } while(rmid-- != index->message_index.begin());
                } while(_rit-- != relevant_entries.begin());
            }
        }
    }

    return max(delete_count, delete_count_volatile);
}

ConversationManager::ConversationManager(const std::shared_ptr<ts::server::VirtualServer> &server) : _ref_server(server) { }

ConversationManager::~ConversationManager() { }

void ConversationManager::initialize(const std::shared_ptr<ConversationManager> &_this) {
    assert(&*_this == this);
    this->_ref_this = _this;

    auto ref_server = this->ref_server();
    assert(ref_server);
    auto sql = ref_server->getSql();
    assert(sql);

    auto result = sql::command(sql, "SELECT `conversation_id`, `file_path` FROM `conversations` WHERE `server_id` = :sid", variable{":sid", ref_server->getServerId()}).query([&](int count, std::string* values, std::string* names) {
        ChannelId conversation_id = 0;
        std::string file_path;

        try {
            for(int index = 0; index < count; index++) {
                if(names[index] == "conversation_id")
                    conversation_id += stoll(values[index]);
                else if(names[index] == "file_path")
                    file_path += values[index];
            }
        } catch(std::exception& ex) {
            logError(ref_server->getServerId(), "[Conversations] Failed to parse conversation entry! Exception: {}", ex.what());
            return 0;
        }

        auto conversation = make_shared<Conversation>(_this, conversation_id, file_path);
        conversation->set_ref_self(conversation);
        string error;
        if(!conversation->initialize(error)) {
            logError(ref_server->getServerId(), "[Conversations] Failed to load conversation for channel {}: {}. Conversation is in volatile mode", conversation_id, error);
        }
        this->_conversations.push_back(conversation);
        return 0;
    });
    LOG_SQL_CMD(result);
    debugMessage(ref_server->getServerId(), "[Conversations] Loaded {} conversations", this->_conversations.size());
}

bool ConversationManager::conversation_exists(ts::ChannelId channel_id) {
    lock_guard lock(this->_conversations_lock);
    return find_if(this->_conversations.begin(), this->_conversations.end(), [&](const shared_ptr<Conversation>& conv){ return conv->channel_id() == channel_id; })!= this->_conversations.end();
}

std::shared_ptr<Conversation> ConversationManager::get(ts::ChannelId channel_id) {
    unique_lock lock(this->_conversations_lock);
    auto found = find_if(this->_conversations.begin(), this->_conversations.end(), [&](const shared_ptr<Conversation>& conv){ return conv->channel_id() == channel_id; });
    if(found != this->_conversations.end())
        return *found;
    return nullptr;
}

std::shared_ptr<Conversation> ConversationManager::get_or_create(ts::ChannelId channel_id) {
    unique_lock lock(this->_conversations_lock);
    auto found = find_if(this->_conversations.begin(), this->_conversations.end(), [&](const shared_ptr<Conversation>& conv){ return conv->channel_id() == channel_id; });
    if(found != this->_conversations.end())
        return *found;

    auto ref_server = this->ref_server();
    assert(ref_server);

    //TODO: More configurable!
    auto file_path = "files/server_" + to_string(ref_server->getServerId()) + "/conversations/conversation_" + to_string(channel_id) + ".cvs";
    auto conversation = make_shared<Conversation>(this->_ref_this.lock(), channel_id, file_path);
    conversation->set_ref_self(conversation);
    this->_conversations.push_back(conversation);
    lock.unlock();

    {
        auto sql_result = sql::command(ref_server->getSql(), "INSERT INTO `conversations` (`server_id`, `channel_id`, `conversation_id`, `file_path`) VALUES (:sid, :cid, :cid, :fpath)",
                variable{":sid", ref_server->getServerId()}, variable{":cid", channel_id}, variable{":fpath", file_path}).executeLater();
        sql_result.waitAndGetLater(LOG_SQL_CMD, {-1, "future error"});
    }

    string error;
    if(!conversation->initialize(error))
        logError(ref_server->getServerId(), "[Conversations] Failed to load conversation for channel {}: {}. Conversation is in volatile mode", channel_id, error);
    return conversation;
}

void ConversationManager::delete_conversation(ts::ChannelId channel_id) {
    {
        lock_guard lock(this->_conversations_lock);
        this->_conversations.erase(remove_if(this->_conversations.begin(), this->_conversations.end(), [&](const shared_ptr<Conversation>& conv){ return conv->channel_id() == channel_id; }), this->_conversations.end());
    }

    auto ref_server = this->ref_server();
    if(!ref_server) {
        logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to delete conversation (server expired)", channel_id);
        return;
    }

    auto sql = ref_server->getSql();
    if(!sql) {
        logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to delete conversation (sql expired)", channel_id);
        return;
    }

    {
        auto sql_result = sql::command(sql, "DELETE FROM `conversations` WHERE `server_id` = :sid AND `channel_id` = :cid AND `conversation_id` = :cid", variable{":sid", ref_server->getServerId()}, variable{":cid", channel_id}).executeLater();
        sql_result.waitAndGetLater(LOG_SQL_CMD, {-1, "future error"});
    }
    {
        auto sql_result = sql::command(sql, "DELETE FROM `conversation_blocks` WHERE `server_id` = :sid AND `conversation_id` = :cid", variable{":sid", ref_server->getServerId()}, variable{":cid", channel_id}).executeLater();
        sql_result.waitAndGetLater(LOG_SQL_CMD, {-1, "future error"});
    }

    //TODO: More configurable!
    auto file_path = "files/server_" + to_string(ref_server->getServerId()) + "/conversations/conversation_" + to_string(channel_id) + ".cvs";
    try {
        fs::remove(fs::u8path(file_path));
    } catch(fs::filesystem_error& error) {
        logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to delete data file ({}): {}|{}", channel_id, file_path, error.code().value(), error.what());
    }
}

void ConversationManager::synchronize_channels() {
    auto ref_server = this->ref_server();
    if(!ref_server) {
        logWarning(ref_server->getServerId(), "[Conversations][{}] Failed to synchronize conversations (server expired)");
        return;
    }

    auto chan_tree = ref_server->getChannelTree();
    deque<shared_ptr<Conversation>> channels{this->_conversations};
    for(auto& channel : channels) {
        auto schannel = chan_tree->findChannel(channel->channel_id());
        if(!schannel) {
            logMessage(ref_server->getServerId(), "[Conversations][{}] Deleting conversation because channel does not exists anymore.", channel->channel_id());
            this->delete_conversation(channel->channel_id());
            continue;
        }

        auto history_size = schannel->properties()[property::CHANNEL_CONVERSATION_HISTORY_LENGTH].as_unchecked<size_t>();
        channel->set_history_length(history_size);
    }
}

void ConversationManager::cleanup_cache() {
    unique_lock lock(this->_conversations_lock);
    std::vector<std::shared_ptr<Conversation>> conversations{this->_conversations.begin(), this->_conversations.end()};
    lock.unlock();

    for(auto& conversation : conversations)
        conversation->cleanup_cache();
}