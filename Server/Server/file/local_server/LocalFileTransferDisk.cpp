//
// Created by WolverinDEV on 04/05/2020.
//

#include <cassert>
#include <event.h>
#include <experimental/filesystem>
#include <log/LogUtils.h>
#include "./LocalFileProvider.h"
#include "./LocalFileTransfer.h"
#include "duration_utils.h"

using namespace ts::server::file;
using namespace ts::server::file::transfer;
namespace fs = std::experimental::filesystem;

DiskIOStartResult LocalFileTransfer::start_disk_io() {
    assert(this->disk_io.state == DiskIOLoopState::STOPPED);

    this->disk_io.state = DiskIOLoopState::RUNNING;
    this->disk_io.dispatch_thread = std::thread(&LocalFileTransfer::dispatch_loop_disk_io, this);
    return DiskIOStartResult::SUCCESS;
}

void LocalFileTransfer::shutdown_disk_io() {
    if(this->disk_io.state == DiskIOLoopState::STOPPED) return;

    this->disk_io.state = DiskIOLoopState::STOPPING;
    {
        std::unique_lock qlock{this->disk_io.queue_lock};
        this->disk_io.notify_work_awaiting.notify_all();
        while(this->disk_io.queue_head) {
            this->disk_io.notify_client_processed.wait_for(qlock, std::chrono::seconds{10});
        }

        if(this->disk_io.queue_head) {
            logWarning(0, "Failed to flush disk IO. Force aborting.");
            this->disk_io.state = DiskIOLoopState::FORCE_STOPPING;
            this->disk_io.notify_work_awaiting.notify_all();
            this->disk_io.notify_client_processed.wait(qlock);
        }
    }

    if(this->disk_io.dispatch_thread.joinable())
        this->disk_io.dispatch_thread.join();

    this->disk_io.state = DiskIOLoopState::STOPPED;
}

void LocalFileTransfer::dispatch_loop_disk_io(void *provider_ptr) {
    auto provider = reinterpret_cast<LocalFileTransfer*>(provider_ptr);

    std::shared_ptr<FileClient> client{};
    while(true) {
        {
            std::unique_lock qlock{provider->disk_io.queue_lock};
            if(client) {
                client->file.currently_processing = false;
                provider->disk_io.notify_client_processed.notify_all();
                client = nullptr;
            }

            provider->disk_io.notify_work_awaiting.wait(qlock, [&]{ return provider->disk_io.state != DiskIOLoopState::RUNNING || provider->disk_io.queue_head != nullptr; });

            if(provider->disk_io.queue_head) {
                try {
                    client = provider->disk_io.queue_head->shared_from_this();
                } catch (std::bad_weak_ptr& ex) {
                    logCritical(LOG_FT, "Disk worker encountered a bad weak ptr. This indicated something went horribly wrong! Please submit this on https://forum.teaspeak.de !!!");
                    client.reset();
                    continue;
                }

                provider->disk_io.queue_head = provider->disk_io.queue_head->file.next_client;
                if(!provider->disk_io.queue_head) {
                    provider->disk_io.queue_tail = &provider->disk_io.queue_head;
                }
            }

            if(provider->disk_io.state != DiskIOLoopState::RUNNING) {
                if(provider->disk_io.state == DiskIOLoopState::STOPPING) {
                    if(!client)
                        break;
                    /* break only if all clients have been flushed */
                } else {
                    /* force stopping without any flushing */
                    auto fclient = &*client;
                    while(fclient) {
                        fclient = std::exchange(fclient->file.next_client, nullptr);
                    }

                    provider->disk_io.queue_head = nullptr;
                    provider->disk_io.queue_tail = &provider->disk_io.queue_head;
                    break;
                }
            }

            if(!client) {
                continue;
            }

            client->file.currently_processing = true;
            client->file.next_client = nullptr;
        }

        provider->execute_disk_io(client);
    }
    provider->disk_io.notify_client_processed.notify_all();
}

bool FileClient::enqueue_disk_buffer_bytes(const void *snd_buffer, size_t size) {
    auto tbuffer = allocate_buffer(size);
    tbuffer->length = size;
    tbuffer->offset = 0;
    memcpy(tbuffer->data, snd_buffer, size);

    size_t buffer_size;
    {
        std::lock_guard block{this->disk_buffer.mutex};
        if(this->disk_buffer.write_disconnected) {
            goto write_disconnected;
        }

        *this->disk_buffer.buffer_tail = tbuffer;
        this->disk_buffer.buffer_tail = &tbuffer->next;
        buffer_size = (this->disk_buffer.bytes += size);
    }

    return buffer_size > TRANSFER_MAX_CACHED_BYTES;

    write_disconnected:
    deref_buffer(tbuffer);
    return false;
}

void FileClient::flush_disk_buffer() {
    Buffer* current_head;
    {
        std::lock_guard block{this->disk_buffer.mutex};

        this->disk_buffer.write_disconnected = true;
        this->disk_buffer.bytes = 0;
        current_head = std::exchange(this->disk_buffer.buffer_head, nullptr);
        this->disk_buffer.buffer_tail = &this->disk_buffer.buffer_head;
    }

    while(current_head) {
        auto next = current_head->next;
        deref_buffer(current_head);
        current_head = next;
    }
}

bool FileClient::buffers_flushed() {
    {
        std::lock_guard db_lock{this->disk_buffer.mutex};
        if(this->disk_buffer.bytes > 0)
            return false;
    }

    {
        std::lock_guard nb_lock{this->network_buffer.mutex};
        if(this->network_buffer.bytes > 0)
            return false;
    }

    return true;
}

FileInitializeResult LocalFileTransfer::initialize_file_io(const std::shared_ptr<FileClient> &transfer) {
    FileInitializeResult result{FileInitializeResult::SUCCESS};
    assert(transfer->transfer);

    std::shared_lock slock{transfer->state_mutex};
    auto& file_data = transfer->file;
    assert(!file_data.file_descriptor);
    assert(!file_data.next_client);

    auto absolute_path = transfer->transfer->absolute_file_path;
    {
        unsigned int open_flags{0};
        if(transfer->transfer->direction == Transfer::DIRECTION_DOWNLOAD) {
            open_flags = O_RDONLY;

            std::error_code fs_error{};
            if(absolute_path.empty() || !fs::exists(absolute_path, fs_error)) {
                result = FileInitializeResult::FILE_DOES_NOT_EXISTS;
                goto error_exit;
            } else if(fs_error) {
                logWarning(LOG_FT, "{} Failed to check for file existence of {}: {}/{}", transfer->log_prefix(), absolute_path, fs_error.value(), fs_error.message());
                result = FileInitializeResult::FILE_SYSTEM_ERROR;
                goto error_exit;
            }
        } else if(transfer->transfer->direction == Transfer::DIRECTION_UPLOAD) {
            std::error_code fs_error{};
            auto parent_path = fs::u8path(absolute_path).parent_path();
            if(!fs::exists(parent_path)) {
                if(!fs::create_directories(parent_path, fs_error) || fs_error) {
                    logError(LOG_FT, "{} Failed to create required directories for file upload for {}: {}/{}", transfer->log_prefix(), absolute_path, fs_error.value(), fs_error.message());
                    result = FileInitializeResult::COUNT_NOT_CREATE_DIRECTORIES;
                    goto error_exit;
                }
            } else if(fs_error) {
                logWarning(LOG_FT, "{} Failed to check for directory existence of {}: {}/{}. Assuming it exists", transfer->log_prefix(), parent_path.string(), fs_error.value(), fs_error.message());
            }

            open_flags = (unsigned) O_WRONLY | (unsigned) O_CREAT;
            if(transfer->transfer->override_exiting)
                open_flags |= (unsigned) O_TRUNC;
        } else {
            return FileInitializeResult::INVALID_TRANSFER_DIRECTION;
        }

        file_data.file_descriptor = open(absolute_path.c_str(), (int) open_flags, 0644);
        if(file_data.file_descriptor <= 0) {
            const auto errno_ = errno;
            switch (errno_) {
                case EACCES:
                    result = FileInitializeResult::FILE_IS_NOT_ACCESSIBLE;
                    break;
                case EDQUOT:
                    logWarning(LOG_FT, "{} Disk inode limit has been reached. Failed to start file transfer for file {}", transfer->log_prefix(), absolute_path);
                    result = FileInitializeResult::FILE_SYSTEM_ERROR;
                    break;
                case EISDIR:
                    result = FileInitializeResult::FILE_IS_A_DIRECTORY;
                    break;
                case EMFILE:
                    result = FileInitializeResult::PROCESS_FILE_LIMIT_REACHED;
                    break;
                case ENFILE:
                    result = FileInitializeResult::SYSTEM_FILE_LIMIT_REACHED;
                    break;
                case ETXTBSY:
                    result = FileInitializeResult::FILE_IS_BUSY;
                    break;
                case EROFS:
                    result = FileInitializeResult::DISK_IS_READ_ONLY;
                    break;
                default:
                    logWarning(LOG_FT, "{} Failed to start file {} for file {}: {}/{}", transfer->log_prefix(),
                               transfer->transfer->direction == Transfer::DIRECTION_DOWNLOAD ? "download" : "upload", absolute_path, errno_, strerror(errno_));
                    result = FileInitializeResult::FILE_SYSTEM_ERROR;
                    break;
            }
            goto error_exit;
        }
    }

    this->file_system_->lock_file(absolute_path);
    transfer->file.file_locked = true;

    if(transfer->transfer->direction == Transfer::DIRECTION_UPLOAD) {
        if(ftruncate(file_data.file_descriptor, transfer->transfer->expected_file_size) != 0) {
            const auto errno_ = errno;
            switch (errno_) {
                case EACCES:
                    logWarning(LOG_FT, "{} File {} got inaccessible on truncating, but not on opening.", transfer->log_prefix(), absolute_path);
                    result = FileInitializeResult::FILE_IS_NOT_ACCESSIBLE;
                    goto error_exit;

                case EFBIG:
                    result = FileInitializeResult::FILE_TOO_LARGE;
                    goto error_exit;

                case EIO:
                    logWarning(LOG_FT, "{} A disk IO error occurred while resizing file {}.", transfer->log_prefix(), absolute_path);
                    result = FileInitializeResult::FILE_IS_NOT_ACCESSIBLE;
                    goto error_exit;

                case EROFS:
                    logWarning(LOG_FT, "{} Failed to resize file {} because disk is in read only mode.", transfer->log_prefix(), absolute_path);
                    result = FileInitializeResult::FILE_IS_NOT_ACCESSIBLE;
                    goto error_exit;
                default:
                    debugMessage(LOG_FT, "{} Failed to truncate file {}: {}/{}. Trying to upload file anyways.", transfer->log_prefix(), absolute_path, errno_, strerror(errno_));
            }
        }
    } else if(transfer->transfer->direction == Transfer::DIRECTION_DOWNLOAD) {
        auto file_size = lseek(file_data.file_descriptor, 0, SEEK_END);
        if(file_size != transfer->transfer->expected_file_size) {
            logWarning(LOG_FT, "{} Expected target file to be of size {}, but file is actually of size {}", transfer->log_prefix(), transfer->transfer->expected_file_size, file_size);
            result = FileInitializeResult::FILE_SIZE_MISMATCH;
            goto error_exit;
        }
        if(transfer->file.query_media_bytes && file_size > 0) {
            auto new_pos = lseek(file_data.file_descriptor, 0, SEEK_SET);
            if(new_pos < 0) {
                logWarning(LOG_FT, "{} Failed to seek to file start: {}/{}", transfer->log_prefix(), errno, strerror(errno));
                result = FileInitializeResult::FILE_SEEK_FAILED;
                goto error_exit;
            }

            auto read = ::read(file_data.file_descriptor, transfer->file.media_bytes, TRANSFER_MEDIA_BYTES_LENGTH);
            if(read <= 0) {
                logWarning(LOG_FT, "{} Failed to read file media bytes: {}/{}", transfer->log_prefix(), errno, strerror(errno));
                result = FileInitializeResult::FAILED_TO_READ_MEDIA_BYTES;
                goto error_exit;
            }
            transfer->file.media_bytes_length = read;
        }
    }

    {
        auto new_pos = lseek(file_data.file_descriptor, transfer->transfer->file_offset, SEEK_SET);
        if(new_pos < 0) {
            logWarning(LOG_FT, "{} Failed to seek to target file offset ({}): {}/{}", transfer->log_prefix(), transfer->transfer->file_offset, errno, strerror(errno));
            result = FileInitializeResult::FILE_SEEK_FAILED;
            goto error_exit;
        } else if(new_pos != transfer->transfer->file_offset) {
            logWarning(LOG_FT, "{} File rw offset mismatch after seek. Expected {} but received {}", transfer->log_prefix(), transfer->transfer->file_offset, new_pos);
            result = FileInitializeResult::FILE_SEEK_FAILED;
            goto error_exit;
        }
        logTrace(LOG_FT, "{} Seek to file offset {}. New actual offset is {}", transfer->log_prefix(), transfer->transfer->file_offset, new_pos);
    }

    return FileInitializeResult::SUCCESS;
    error_exit:
    if(std::exchange(transfer->file.file_locked, false))
        this->file_system_->unlock_file(absolute_path);

    if(file_data.file_descriptor > 0)
        ::close(file_data.file_descriptor);
    file_data.file_descriptor = 0;

    return result;
}

void LocalFileTransfer::finalize_file_io(const std::shared_ptr<FileClient> &transfer,
                                         std::unique_lock<std::shared_mutex> &state_lock) {
    assert(state_lock.owns_lock());
    if(!transfer->transfer) {
        return;
    }

    auto absolute_path = transfer->transfer->absolute_file_path;

    auto& file_data = transfer->file;

    state_lock.unlock();
    {
        std::unique_lock dlock{this->disk_io.queue_lock};
        while(true) {
            if(file_data.currently_processing) {
                this->disk_io.notify_client_processed.wait(dlock);
                continue;
            }

            if(this->disk_io.queue_head == &*transfer) {
                this->disk_io.queue_head = file_data.next_client;
                if (!this->disk_io.queue_head) {
                    this->disk_io.queue_tail = &this->disk_io.queue_head;
                }
            } else if(file_data.next_client || this->disk_io.queue_tail == &file_data.next_client) {
                FileClient* head{this->disk_io.queue_head};
                while(head->file.next_client != &*transfer) {
                    assert(head->file.next_client);
                    head = head->file.next_client;
                }

                head->file.next_client = file_data.next_client;
                if(!file_data.next_client)
                    this->disk_io.queue_tail = &head->file.next_client;
            }
            file_data.next_client = nullptr;
            break;
        }
    }
    state_lock.lock();

    if(std::exchange(file_data.file_locked, false)) {
        this->file_system_->unlock_file(absolute_path);
    }

    if(file_data.file_descriptor > 0)
        ::close(file_data.file_descriptor);
    file_data.file_descriptor = 0;
}

void LocalFileTransfer::enqueue_disk_io(const std::shared_ptr<FileClient> &client) {
    if(!client->file.file_descriptor) {
        return;
    }

    if(!client->transfer) {
        return;
    }

    if(client->transfer->direction == Transfer::DIRECTION_DOWNLOAD) {
        if(client->state != FileClient::STATE_TRANSFERRING) {
            return;
        }

        if(client->disk_buffer.bytes > TRANSFER_MAX_CACHED_BYTES) {
            return;
        }
    } else if(client->transfer->direction == Transfer::DIRECTION_UPLOAD) {
        /* we don't do this check because this might be a flush instruction, where the buffer is actually zero bytes filled */
        /*
        if(client->buffer.bytes == 0)
            return;
        */
    }

    std::lock_guard dlock{this->disk_io.queue_lock};
    if(client->file.next_client || this->disk_io.queue_tail == &client->file.next_client) {
        return;
    }

    *this->disk_io.queue_tail = &*client;
    this->disk_io.queue_tail = &client->file.next_client;

    this->disk_io.notify_work_awaiting.notify_all();
}

void LocalFileTransfer::execute_disk_io(const std::shared_ptr<FileClient> &client) {
    if(!client->transfer) {
        return;
    }

    if(client->transfer->direction == Transfer::DIRECTION_UPLOAD) {
        Buffer* buffer;
        size_t buffer_left_size;

        while(true) {
            {
                std::lock_guard block{client->disk_buffer.mutex};

                if(!client->disk_buffer.buffer_head) {
                    assert(client->disk_buffer.bytes == 0);
                    buffer_left_size = 0;
                    break;
                }

                buffer_left_size = client->disk_buffer.bytes;
                buffer = ref_buffer(client->disk_buffer.buffer_head);
            }

            assert(buffer->offset < buffer->length);
            auto written = ::write(client->file.file_descriptor, buffer->data + buffer->offset, buffer->length - buffer->offset);
            if(written <= 0) {
                deref_buffer(buffer);

                if(errno == EAGAIN) {
                    //TODO: Timeout?
                    this->enqueue_disk_io(client);
                    break;
                }

                if(written == 0) {
                    /* EOF, how the hell is this event possible?! */
                    auto offset_written = client->statistics.disk_bytes_write.total_bytes + client->transfer->file_offset;
                    auto aoffset = lseek(client->file.file_descriptor, 0, SEEK_CUR);
                    logError(LOG_FT, "{} Received unexpected file write EOF. EOF received at {} but expected {}. Actual file offset: {}. Closing transfer.",
                            client->log_prefix(), offset_written, client->transfer->expected_file_size, aoffset);


                    this->invoke_aborted_callback(client, { TransferError::UNEXPECTED_DISK_EOF, strerror(errno) });

                    client->flush_disk_buffer();
                    {
                        std::unique_lock slock{client->state_mutex};
                        client->handle->disconnect_client(client, slock, true);
                    }
                } else {
                    auto offset_written = client->statistics.disk_bytes_write.total_bytes + client->transfer->file_offset;
                    auto aoffset = lseek(client->file.file_descriptor, 0, SEEK_CUR);
                    logError(LOG_FT, "{} Received write to disk IO error. Write pointer is at {} of {}. Actual file offset: {}. Closing transfer.",
                            client->log_prefix(), offset_written, client->transfer->expected_file_size, aoffset);

                    this->invoke_aborted_callback(client, { TransferError::DISK_IO_ERROR, strerror(errno) });

                    client->flush_disk_buffer();
                    {
                        std::unique_lock slock{client->state_mutex};
                        client->handle->disconnect_client(client, slock, true);
                    }
                }
                return;
            } else {
                buffer->offset += written;
                assert(buffer->offset <= buffer->length);

                if(buffer->length == buffer->offset) {
                    {
                        std::lock_guard block{client->disk_buffer.mutex};
                        if(client->disk_buffer.buffer_head == buffer) {
                            client->disk_buffer.buffer_head = buffer->next;
                            if(!buffer->next) {
                                client->disk_buffer.buffer_tail = &client->disk_buffer.buffer_head;
                            }

                            assert(client->disk_buffer.bytes >= written);
                            client->disk_buffer.bytes -= written;
                            buffer_left_size = client->disk_buffer.bytes;

                            /* We have to deref the buffer twice since we've removed it from the list which owns us one reference */
                            /* Will not trigger a memory free since we're still holding onto one reference */
                            deref_buffer(buffer);
                        } else {
                            /* The buffer got removed */
                        }
                    }
                } else {
                    std::lock_guard block{client->disk_buffer.mutex};
                    if(client->disk_buffer.buffer_head == buffer) {
                        assert(client->disk_buffer.bytes >= written);
                        client->disk_buffer.bytes -= written;
                        buffer_left_size = client->disk_buffer.bytes;
                    } else {
                        /* The buffer got removed */
                    }
                }

                deref_buffer(buffer);
                client->statistics.disk_bytes_write.increase_bytes(written);
            }
        }

        if(buffer_left_size > 0) {
            this->enqueue_disk_io(client);
        } else if(client->state == FileClient::STATE_FLUSHING) {
            this->test_disconnecting_state(client);
        }

        if(client->state == FileClient::STATE_TRANSFERRING && buffer_left_size < TRANSFER_MAX_CACHED_BYTES / 2) {
            if(client->disk_buffer.buffering_stopped) {
                logMessage(LOG_FT, "{} Starting network read, buffer is capable for reading again.", client->log_prefix());
            }

            client->add_network_read_event(false);
        }
    } else if(client->transfer->direction == Transfer::DIRECTION_DOWNLOAD) {
        if(client->state == FileClient::STATE_FLUSHING) {
            client->flush_disk_buffer(); /* just in case, file download usually don't write to the disk */
            return;
        }

        while(true) {
            constexpr auto buffer_capacity{4096};
            char buffer[buffer_capacity];

            auto read = ::read(client->file.file_descriptor, buffer, buffer_capacity);
            if(read <= 0) {
                if(errno == EAGAIN) {
                    this->enqueue_disk_io(client);
                    return;
                }

                if(read == 0) {
                    /* EOF */
                    auto offset_send = client->statistics.disk_bytes_read.total_bytes + client->transfer->file_offset;
                    if(client->transfer->expected_file_size == offset_send) {
                        debugMessage(LOG_FT, "{} Finished file reading. Flushing and disconnecting transfer. Reading took {} seconds.",
                                client->log_prefix(), duration_to_string(std::chrono::system_clock::now() - client->timings.key_received));
                    } else {
                        auto aoffset = lseek(client->file.file_descriptor, 0, SEEK_CUR);
                        logError(LOG_FT, "{} Received unexpected read EOF. EOF received at {} but expected {}. Actual file offset: {}. Disconnecting client.",
                                client->log_prefix(), offset_send, client->transfer->expected_file_size, aoffset);

                        this->invoke_aborted_callback(client, { TransferError::UNEXPECTED_DISK_EOF, "" });
                    }
                } else {
                    logWarning(LOG_FT, "{} Failed to read from file {} ({}/{}). Aborting transfer.", client->log_prefix(), client->transfer->absolute_file_path, errno, strerror(errno));

                    this->invoke_aborted_callback(client, { TransferError::DISK_IO_ERROR, strerror(errno) });
                }

                std::unique_lock slock{client->state_mutex};
                client->handle->disconnect_client(client, slock, true);
                return;
            } else {
                auto buffer_full = client->send_file_bytes(buffer, read);
                client->statistics.disk_bytes_read.increase_bytes(read);
                client->statistics.file_transferred.increase_bytes(read);

                std::shared_lock slock{client->state_mutex};
                if(buffer_full) {
                    //logTrace(LOG_FT, "{} Stopping buffering from disk. Buffer full ({}bytes)", client->log_prefix(), client->buffer.bytes);
                    break;
                }

                /* we've stuff to write again, yeahr */
                client->add_network_write_event(false);
            }
        }
    } else {
        logError(LOG_FT, "{} Disk IO scheduled, but transfer direction is unknown.", client->log_prefix());
    }
}