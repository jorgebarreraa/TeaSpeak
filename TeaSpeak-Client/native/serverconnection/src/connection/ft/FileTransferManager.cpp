#include "FileTransferManager.h"
#include "FileTransferObject.h"

#include <misc/net.h>
#include <algorithm>
#include <utility>

#ifndef WIN32
    #include <unistd.h>
	#include <misc/net.h>

	#ifndef IPPROTO_TCP
        #define IPPROTO_TCP (0)
	#endif
#else
    #include <ws2tcpip.h>

    #define SOCK_NONBLOCK (0)
    #define MSG_DONTWAIT (0)
#endif

using namespace tc;
using namespace tc::ft;
using namespace std;
using namespace std::chrono;

tc::ft::FileTransferManager* transfer_manager = nullptr;

Transfer::~Transfer() {
	log_free("Transfer", this);
}

bool Transfer::initialize(std::string &error) {
	if(this->_state != state::UNINITIALIZED) {
		error = tr("invalid state");
		return false;
	}

	if(!this->_transfer_object->initialize(error)) {
		error = tr("failed to initialize transfer object: ") + error;
		return false;
	}
	this->_state = state::CONNECTING;

	/* resolve address */
	{
		addrinfo hints{}, *result;
		memset(&hints, 0, sizeof(hints));

		hints.ai_family = AF_UNSPEC;

		if(getaddrinfo(this->_options->remote_address.data(), nullptr, &hints, &result) != 0 || !result) {
			error = tr("failed to resolve hostname");
			this->_state = state::UNINITIALIZED;
			return false;
		}

		memcpy(&this->remote_address, result->ai_addr, result->ai_addrlen);
		freeaddrinfo(result);
	}
	switch(this->remote_address.ss_family) {
		case AF_INET:
			((sockaddr_in*) &this->remote_address)->sin_port = htons(this->_options->remote_port);
			break;
		case AF_INET6:
			((sockaddr_in6*) &this->remote_address)->sin6_port = htons(this->_options->remote_port);
            break;
		default:
		    log_warn(category::file_transfer, tr("getaddrinfo() returns unknown address family ({})"), this->remote_address.ss_family);
			break;
	}

	log_info(category::file_transfer, tr("Setting remote port to {}"), net::to_string(this->remote_address));
	this->_socket = (int) ::socket(this->remote_address.ss_family, (unsigned) SOCK_STREAM | (unsigned) SOCK_NONBLOCK, IPPROTO_TCP);
	if(this->_socket < 0) {
		this->finalize(true, true);
		error = tr("failed to spawn socket");
		return false;
	}

#ifdef WIN32
    u_long enabled = 1;
    auto non_block_rs = ioctlsocket(this->_socket, FIONBIO, &enabled);
    if (non_block_rs != NO_ERROR) {
        this->finalize(true, true);
        error = "failed to enable non blocking more";
        return false;
    }
#endif

	{

		lock_guard lock(this->event_lock);
		this->event_read = event_new(this->event_io, this->_socket, (unsigned) EV_READ | (unsigned) EV_PERSIST, &Transfer::_callback_read, this);
		this->event_write = event_new(this->event_io, this->_socket, EV_WRITE, &Transfer::_callback_write, this);
	}
	return true;
}

bool Transfer::connect() {
	int result = ::connect(this->_socket, reinterpret_cast<sockaddr *> (&this->remote_address), sizeof(this->remote_address));

	if (result < 0) {
#ifdef WIN32
        auto error = WSAGetLastError();

        if(error != WSAEWOULDBLOCK) {
            wchar_t *s = nullptr;
            FormatMessageW(
                    (DWORD) FORMAT_MESSAGE_ALLOCATE_BUFFER | (DWORD) FORMAT_MESSAGE_FROM_SYSTEM | (DWORD) FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr,
                    error,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPWSTR)&s,
                    0,
                    nullptr
            );
            auto r = wcschr(s, L'\r');
            if(r) *r = L'\0';
            
            this->call_callback_failed(std::to_string(error) + "/" + std::string{s, s + wcslen(s)});
            log_trace(category::file_transfer, tr("Failed to connect with code: {} => {}/{}"), result, error, std::string{s, s + wcslen(s)}.c_str());
            LocalFree(s);
            
            this->finalize(true, true);
            return false;
        }
#else
		if(errno != EINPROGRESS) {
			log_error(category::file_transfer, tr("Failed to connect with code: {} => {}/{}"), result, errno, strerror(errno));
            this->call_callback_failed(to_string(errno) + "/" +  strerror(errno));
			this->finalize(true, true);
			return false;
		}
#endif
	} else {
		this->_state = state::CONNECTED; /* we're connected */
	}
	log_debug(category::file_transfer, tr("Connect result: {} - {}"), result, errno);

	timeval connect_timeout{5, 0};
	event_add(this->event_write, &connect_timeout); /* enabled if socket is connected */
	////event_add(this->event_read, &connect_timeout); /* enabled if socket is connected */

	if(this->_state == state::CONNECTED)
		this->handle_connected();
	return true;
}

void Transfer::finalize(bool blocking, bool aborted) {
	if(this->_state == state::UNINITIALIZED)
		return;

	this->_state = state::UNINITIALIZED;

	{
		unique_lock lock(this->event_lock);
		auto ev_read = std::exchange(this->event_read, nullptr);
        auto ev_write = std::exchange(this->event_write, nullptr);
		lock.unlock();

		if(ev_read) {
			if(blocking)
				event_del_block(ev_read);
			else
				event_del_noblock(ev_read);
			event_free(ev_read);
		}
		if(ev_write) {
			if(blocking)
				event_del_block(ev_write);
			else
				event_del_noblock(ev_write);
			event_free(ev_write);
		}
	}

	if(this->_socket > 0) {
#ifdef WIN32
        closesocket(this->_socket);
#else
		shutdown(this->_socket, SHUT_RDWR);
		close(this->_socket);
#endif
		this->_socket = 0;
	}

	this->_transfer_object->finalize(aborted);
	this->_handle->remove_transfer(this);
}

void Transfer::_callback_write(evutil_socket_t, short flags, void *_ptr_transfer) {
	reinterpret_cast<Transfer*>(_ptr_transfer)->callback_write(flags);
}

void Transfer::_callback_read(evutil_socket_t, short flags, void *_ptr_transfer) {
	reinterpret_cast<Transfer*>(_ptr_transfer)->callback_read(flags);
}

void Transfer::callback_read(short flags) {
	if(this->_state < state::CONNECTING && this->_state > state::DISCONNECTING)
		return;

	if((unsigned) flags & (unsigned) EV_TIMEOUT) {
		auto target = dynamic_pointer_cast<TransferTarget>(this->_transfer_object);
		if(target) {
		    if(this->last_target_write.time_since_epoch().count() == 0) {
                this->last_target_write = system_clock::now();
		    } else if(system_clock::now() - this->last_target_write > seconds(5)) {
				this->call_callback_failed("timeout (write)");
				this->finalize(false, true);
				return;
			}
		} else {
            if(this->last_source_read.time_since_epoch().count() == 0)
                this->last_source_read = system_clock::now();
            else if(system_clock::now() - this->last_source_read > seconds(5)) {
				this->call_callback_failed("timeout (read)");
				this->finalize(false, true);
				return;
			}
		}

		{
			lock_guard lock(this->event_lock);
			if(this->event_read) {
				event_add(this->event_read, &this->alive_check_timeout);
			}
		}
	}

	if((unsigned) flags & (unsigned) EV_READ) {
		if(this->_state == state::CONNECTING) {
			log_debug(category::file_transfer, tr("Connected (read event)"));
			this->handle_connected();
		}

		int64_t buffer_length = 1024;
		char buffer[1024];

		buffer_length = recv(this->_socket, buffer, (int) buffer_length, MSG_DONTWAIT);
		if(buffer_length < 0) {
#ifdef WIN32
            auto error = WSAGetLastError();

            if(error != WSAEWOULDBLOCK)
                return;
#else
            if(errno == EAGAIN)
				return;
#endif

			log_error(category::file_transfer, tr("Received an error while receiving data: {}/{}"), errno, strerror(errno));
			//TODO may handle this error message?
			this->handle_disconnect(false);
			return;
		} else if(buffer_length == 0) {
			log_info(category::file_transfer, tr("Received an disconnect"));
			this->handle_disconnect(false);
			return;
		}

		auto target = dynamic_pointer_cast<TransferTarget>(this->_transfer_object);
		if(target) {
			string error;
			auto state = target->write_bytes(error, (uint8_t*) buffer, buffer_length);
			this->last_target_write = system_clock::now();
			if(state == error::out_of_space) {
				log_error(category::file_transfer, tr("Failed to write read data (out of space)"));
				this->call_callback_failed(tr("out of local space"));
				this->finalize(true, true);
				return;
			} else if(state == error::custom) {
				log_error(category::file_transfer, tr("Failed to write read data ({})"), error);
				this->call_callback_failed(error);
				this->finalize(true, true);
				return;
			} else if(state == error::custom_recoverable) {
				log_error(category::file_transfer, tr("Failed to write read data ({})"), error);
			} else if(state != error::success) {
				log_error(category::file_transfer, tr("invalid local write return code! ({})"), state);
			}

			auto stream_index = target->stream_index();
			auto expected_bytes = target->expected_length();
			if(stream_index >= expected_bytes) {
				this->call_callback_finished(false);
				this->finalize(false, false);
			}
			this->call_callback_process(stream_index, expected_bytes);
		} else {
			log_warn(category::file_transfer, tr("Read {} bytes, but we're not in download mode"), buffer_length);
		}
	}
}

void Transfer::callback_write(short flags) {
	if(this->_state < state::CONNECTING && this->_state > state::DISCONNECTING)
		return;

	if((unsigned) flags & (unsigned) EV_TIMEOUT) {
		//we received a timeout! (May just for creating buffers)
		if(this->_state == state::CONNECTING) {
			this->call_callback_failed(tr("failed to connect"));
			this->finalize(false, true);
			return;
		}
	}

	bool readd_write = false, readd_write_for_read = false;
	if((unsigned) flags & (unsigned) EV_WRITE) {
		if(this->_state == state::CONNECTING)
			this->handle_connected();

		pipes::buffer buffer;
		while(true) {
			{
				lock_guard lock(this->queue_lock);
				auto size = this->write_queue.size();
				if(size == 0)
					break;

				buffer = this->write_queue.front();
				this->write_queue.pop_front();

				readd_write = size > 1;
			}

			auto written = send(this->_socket, buffer.data_ptr<char>(), (int) buffer.length(), MSG_DONTWAIT);
			if(written <= 0) {
				{
					lock_guard lock(this->queue_lock);
					this->write_queue.push_front(buffer);
					readd_write = true;
				}
#ifdef WIN32
                auto _error = WSAGetLastError();
#else
                auto _error = errno;
                #define WSAEWOULDBLOCK (0)
                #define WSAECONNREFUSED (0)
                #define WSAECONNRESET (0)
                #define WSAENOTCONN (0)
#endif
				if(_error == EAGAIN || _error == WSAEWOULDBLOCK) {
					break; /* event will be added with e.g. a timeout */
				} else if(_error == ECONNREFUSED || _error == WSAECONNREFUSED) {
					this->call_callback_failed(tr("connection refused"));
					this->finalize(false, true);
				} else if(_error == ECONNRESET || _error == WSAECONNRESET) {
                    this->call_callback_failed(tr("connection reset"));
                    this->finalize(false, true);
                } else if(_error ==  ENOTCONN || _error == WSAENOTCONN) {
                    this->call_callback_failed(tr("not connected"));
                    this->finalize(false, true);
				} else if(written == 0) {
					this->handle_disconnect(true);
				} else {
					log_error(category::file_transfer, tr("Encountered write error: {}/{}"), _error, strerror(_error));
					this->handle_disconnect(true);
				}
				return;
			}

			if(written < buffer.length()) {
				lock_guard lock(this->queue_lock);
				this->write_queue.push_front(buffer.range(written));
				readd_write = true;
			}
		}
	}

	if(this->_state == state::CONNECTED) {
		auto source = dynamic_pointer_cast<TransferSource>(this->_transfer_object);
		if(source) {
			size_t queue_length = 0;
			{
				lock_guard lock(this->queue_lock);
				queue_length = this->write_queue.size();
			}


			string error;
			auto total_bytes = source->byte_length();
			auto bytes_to_write = total_bytes - source->stream_index();

			while(queue_length < 8 && bytes_to_write > 0) {
				uint64_t buffer_size = 1400; /* best TCP packet size (equal to the MTU) */
				pipes::buffer buffer{buffer_size};
				auto read_status = source->read_bytes(error, buffer.data_ptr<uint8_t>(), buffer_size);
				if(read_status != error::success) {
					if(read_status == error::would_block) {
						readd_write_for_read = true;
						break;
					} else if(read_status == error::custom) {
						this->call_callback_failed(tr("failed to read from source: ") + error);
						this->finalize(false, true);
						return;
					} else if(read_status == error::custom_recoverable) {
						log_warn(category::file_transfer, tr("Failed to read from source (but its recoverable): {}"), error);
						break;
					} else {
						log_error(category::file_transfer, tr("invalid source read status ({})"), read_status);
						this->finalize(false, true);
						return;
					}
				} else if(buffer_size == 0) {
					log_warn(category::file_transfer, tr("Invalid source read size! ({})"), buffer_size);
					break;
				}

                this->last_source_read = system_clock::now();
				{
					lock_guard lock(this->queue_lock);
					this->write_queue.push_back(buffer.range(0, buffer_size));
					queue_length = this->write_queue.size();
				}

				bytes_to_write -= buffer_size;
			}

			this->call_callback_process(total_bytes - bytes_to_write, total_bytes);
			if(queue_length == 0) {
				if(source->stream_index() == source->byte_length()) {
					this->call_callback_finished(false);
					this->finalize(false, false);
					return;
				}
			}


			readd_write = queue_length > 0;
		}
	}

	/* we only need write for connect */
	if(readd_write || readd_write_for_read) {
		lock_guard lock(this->event_lock);
		if(this->event_write) {
			timeval timeout{};
			if(readd_write) {
				/* we should be writeable within the next second or we do a keep alive circle */
				timeout.tv_sec = 1;
				timeout.tv_usec = 0;
			} else if(readd_write_for_read) {
				/* Schedule a next read attempt of our source */
				timeout.tv_sec = 0;
				timeout.tv_usec = 50000;
			}
			event_add(this->event_write, &timeout);
		}
	}
}

void Transfer::_write_message(const pipes::buffer_view &buffer) {
	{
		lock_guard lock(this->queue_lock);
		this->write_queue.push_back(buffer.own_buffer());
	}
	if(this->_state >= state::CONNECTED) {
		lock_guard lock(this->event_lock);
		if(this->event_write) {
			event_add(this->event_write, nullptr);
		}
	}
}

void Transfer::handle_disconnect(bool write_disconnect) {
	if(this->_state != state::DISCONNECTING) {
		auto source = dynamic_pointer_cast<TransferSource>(this->_transfer_object);
		auto target = dynamic_pointer_cast<TransferTarget>(this->_transfer_object);

		auto mode = std::string{write_disconnect ? "write" : "read"};
		if(source && source->stream_index() != source->byte_length()) {
			this->call_callback_failed("received " + mode + " disconnect while transmitting data (" + to_string(source->stream_index()) + "/" + to_string(source->byte_length()) +  ")");
		} else if(target && target->stream_index() != target->expected_length()) {
			this->call_callback_failed("received " + mode + " disconnect while receiving data (" + to_string(target->stream_index()) + "/" + to_string(target->expected_length()) +  ")");
		} else
			this->call_callback_finished(false);
	}
	this->finalize(false, false);
}

void Transfer::handle_connected() {
	log_info(category::file_transfer, tr("Transfer connected. Sending key {}"), this->_options->transfer_key);

	this->_state = state::CONNECTED;
	event_add(this->event_read, &this->alive_check_timeout);

	this->_write_message(pipes::buffer_view{this->_options->transfer_key.data(), this->_options->transfer_key.length()});
	this->call_callback_connected();
	//We dont have to add a timeout to write for prebuffering because its already done by writing this message
}

void Transfer::call_callback_connected() {
	if(this->callback_start)
		this->callback_start();
}

void Transfer::call_callback_failed(const std::string &error) {
	if(this->callback_failed)
		this->callback_failed(error);
}

void Transfer::call_callback_finished(bool aborted) {
	if(this->callback_finished)
		this->callback_finished(aborted);
}

void Transfer::call_callback_process(size_t current, size_t max) {
	auto now = system_clock::now();
	if(now - milliseconds{500} > this->last_process_call)
		this->last_process_call = now;
	else
		return;
	if(this->callback_process)
		this->callback_process(current, max);
}

FileTransferManager::FileTransferManager() = default;
FileTransferManager::~FileTransferManager() = default;

void FileTransferManager::initialize() {
	this->event_io_canceled = false;
	this->event_io = event_base_new();
	this->event_io_thread = std::thread(&FileTransferManager::_execute_event_loop, this);
}

bool save_join(std::thread &thread, bool rd) {
    try {
        if(thread.joinable())
            thread.join();
    } catch(const std::system_error& ex) {
        if(ex.code() == std::errc::resource_deadlock_would_occur) {
            if(rd)
                return false;
            throw;
        } else if(ex.code() == std::errc::no_such_process) {
            return false;
        } else if(ex.code() == std::errc::invalid_argument) {
            return false;
        } else {
            throw;
        }
    }
    return true;
}


void FileTransferManager::finalize() {
	this->event_io_canceled = true;

	event_base_loopbreak(this->event_io);
	save_join(this->event_io_thread, false);

	//TODO drop all file transfers!
	event_base_free(this->event_io);
	this->event_io = nullptr;
}

void FileTransferManager::_execute_event_loop() {
    while(!this->event_io_canceled)
        event_base_loop(this->event_io, EVLOOP_NO_EXIT_ON_EMPTY);
}

std::shared_ptr<Transfer> FileTransferManager::register_transfer(std::string& error, const std::shared_ptr<tc::ft::TransferObject> &object, std::unique_ptr<tc::ft::TransferOptions> options) {
	auto transfer = make_shared<Transfer>(this, object, move(options));

	transfer->event_io = this->event_io;
	if(!transfer->initialize(error)) {
		//error = "failed to initialize transfer: " + error;
		return nullptr;
	}
	{
		lock_guard lock(this->_transfer_lock);
		this->_running_transfers.push_back(transfer);
	}

	return transfer;
}

void FileTransferManager::drop_transfer(const std::shared_ptr<Transfer> &transfer) {
	transfer->finalize(true, true);
	{
		lock_guard lock(this->_transfer_lock);
		auto it = find(this->_running_transfers.begin(), this->_running_transfers.end(), transfer);
		if(it != this->_running_transfers.end())
			this->_running_transfers.erase(it);
	}
}

void FileTransferManager::remove_transfer(tc::ft::Transfer *transfer) {
	lock_guard lock(this->_transfer_lock);
	this->_running_transfers.erase(remove_if(this->_running_transfers.begin(), this->_running_transfers.end(), [&](const shared_ptr<Transfer>& _t) {
		return &*_t == transfer;
	}), this->_running_transfers.end());
}

#ifdef NODEJS_API
#include <NanGet.h>

NAN_MODULE_INIT(JSTransfer::Init) {
	auto klass = Nan::New<v8::FunctionTemplate>(JSTransfer::NewInstance);
	klass->SetClassName(Nan::New("JSTransfer").ToLocalChecked());
	klass->InstanceTemplate()->SetInternalFieldCount(1);

	Nan::SetPrototypeMethod(klass, "start", JSTransfer::_start);

	constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(JSTransfer::NewInstance) {
	if (info.IsConstructCall()) {
		if(info.Length() != 1 || !info[0]->IsObject()) {
			Nan::ThrowError("invalid arguments");
			return;
		}

		/*
		 *  transfer_key: string;
		 *  client_transfer_id: number;
		 *  server_transfer_id: number;
		 *  object: HandledTransferObject;
		 */
		auto options = info[0]->ToObject(Nan::GetCurrentContext()).ToLocalChecked();
		auto key = Nan::GetStringLocal(options, "transfer_key");
		v8::Local<v8::Number> client_transfer_id = Nan::GetLocal<v8::Number>(options, "client_transfer_id");
		v8::Local<v8::Number> server_transfer_id = Nan::GetLocal<v8::Number>(options, "server_transfer_id");
		v8::Local<v8::String> remote_address = Nan::GetStringLocal(options, "remote_address");
		v8::Local<v8::Number> remote_port = Nan::GetLocal<v8::Number>(options, "remote_port");

		if(
				key.IsEmpty() || !key->IsString() ||
				remote_address.IsEmpty() || !remote_address->IsString() ||
				remote_port.IsEmpty() || !remote_port->IsInt32() ||
				client_transfer_id.IsEmpty() || !client_transfer_id->IsInt32() ||
				server_transfer_id.IsEmpty() || !server_transfer_id->IsInt32()
		) {
			Nan::ThrowError("invalid argument types");
			return;
		}

		auto wrapped_options = Nan::GetLocal<v8::Object>(options, "object");
		if(!TransferObjectWrap::is_wrap(wrapped_options)) {
			Nan::ThrowError("invalid handle");
			return;
		}
		auto transfer_object = ObjectWrap::Unwrap<TransferObjectWrap>(wrapped_options)->target();
		assert(transfer_object);


		auto t_options = make_unique<TransferOptions>();
		t_options->transfer_key = *Nan::Utf8String(key);
		t_options->client_transfer_id = client_transfer_id->Int32Value(Nan::GetCurrentContext()).FromMaybe(0);
		t_options->server_transfer_id = server_transfer_id->Int32Value(Nan::GetCurrentContext()).FromMaybe(0);
		t_options->remote_address = *Nan::Utf8String(remote_address);
		t_options->remote_port = remote_port->Int32Value(Nan::GetCurrentContext()).FromMaybe(0);

		string error;
		auto transfer = transfer_manager->register_transfer(error, transfer_object, move(t_options));
		if(!transfer) {
			Nan::ThrowError(Nan::New<v8::String>("failed to create transfer: " + error).ToLocalChecked());
			return;
		}

		auto js_instance = new JSTransfer(transfer);
		js_instance->Wrap(info.This());
		js_instance->_self_ref = true;
		js_instance->Ref(); /* increase ref counter because file transfer is running */
		info.GetReturnValue().Set(info.This());
	} else {
		if(info.Length() != 1) {
			Nan::ThrowError("invalid argument count");
			return;
		}

		v8::Local<v8::Function> cons = Nan::New(constructor());
		v8::Local<v8::Value> argv[1] = {info[0]};

		Nan::TryCatch try_catch;
		auto result = Nan::NewInstance(cons, info.Length(), argv);
		if(try_catch.HasCaught()) {
			try_catch.ReThrow();
			return;
		}
		info.GetReturnValue().Set(result.ToLocalChecked());
	}
}

JSTransfer::JSTransfer(std::shared_ptr<tc::ft::Transfer> transfer) : _transfer(move(transfer)) {
    log_allocate("JSTransfer", this);
	this->call_failed = Nan::async_callback([&](std::string error) {
		Nan::HandleScope scope;
		this->callback_failed(std::move(error));
	});
	this->call_finished = Nan::async_callback([&](bool f) {
		Nan::HandleScope scope;
		this->callback_finished(f);
	});
	this->call_start = Nan::async_callback([&] {
		Nan::HandleScope scope;
		this->callback_start();
	});
	this->call_progress = Nan::async_callback([&](uint64_t a, uint64_t b) {
		Nan::HandleScope scope;
		this->callback_progress(a, b);
	});

	this->_transfer->callback_failed = [&](std::string error) { this->call_failed(std::forward<string>(error)); };
	this->_transfer->callback_finished = [&](bool f) { this->call_finished(std::forward<bool>(f)); };
	this->_transfer->callback_start = [&] { this->call_start(); };
	this->_transfer->callback_process = [&](uint64_t a, uint64_t b) { this->call_progress.call_cpy(a, b, true); };
}

JSTransfer::~JSTransfer() {
    log_free("JSTransfer", this);
	this->_transfer->callback_failed = nullptr;
	this->_transfer->callback_finished = nullptr;
	this->_transfer->callback_start = nullptr;
	this->_transfer->callback_process = nullptr;
}

NAN_METHOD(JSTransfer::destory_transfer) {
	//TODO!
	Nan::ThrowError("Not implemented!");
}

NAN_METHOD(JSTransfer::_start) {
	return ObjectWrap::Unwrap<JSTransfer>(info.Holder())->start(info);
}
NAN_METHOD(JSTransfer::start) {
	if(!this->_transfer->connect()) {
	    log_debug(category::file_transfer, tr("Failed to start file transfer. Error callback should be called!"));
	    info.GetReturnValue().Set(Nan::New<v8::Boolean>(false));
		return;
	}

	log_info(category::file_transfer, tr("Connecting to {}:{}"), this->_transfer->options().remote_address, this->_transfer->options().remote_port);
    info.GetReturnValue().Set(Nan::New<v8::Boolean>(true));
}

NAN_METHOD(JSTransfer::_abort) {
	return ObjectWrap::Unwrap<JSTransfer>(info.Holder())->abort(info);
}
NAN_METHOD(JSTransfer::abort) {
	//TODO!
	Nan::ThrowError("Not implemented");
}

void JSTransfer::callback_finished(bool flag) {
	if(this->_self_ref) {
		this->_self_ref = false;
		this->Unref();
	}

	auto callback = Nan::Get(this->handle(), Nan::New<v8::String>("callback_finished").ToLocalChecked()).ToLocalChecked().As<v8::Function>();
	if(callback.IsEmpty() || !callback->IsFunction())
		return;

	v8::Local<v8::Value> arguments[1];
	arguments[0] = Nan::New<v8::Boolean>(flag);
    (void) callback->Call(Nan::GetCurrentContext(), Nan::Undefined(), 1, arguments);
}

void JSTransfer::callback_start() {
	auto callback = Nan::Get(this->handle(), Nan::New<v8::String>("callback_start").ToLocalChecked()).ToLocalChecked().As<v8::Function>();
	if(callback.IsEmpty() || !callback->IsFunction())
		return;

    (void) callback->Call(Nan::GetCurrentContext(), Nan::Undefined(), 0, nullptr);
}

void JSTransfer::callback_progress(uint64_t a, uint64_t b) {
	auto callback = Nan::Get(this->handle(), Nan::New<v8::String>("callback_progress").ToLocalChecked()).ToLocalChecked().As<v8::Function>();
	if(callback.IsEmpty() || !callback->IsFunction())
		return;

	v8::Local<v8::Value> arguments[2];
	arguments[0] = Nan::New<v8::Number>((uint32_t) a);
	arguments[1] = Nan::New<v8::Number>((uint32_t) b);
    (void) callback->Call(Nan::GetCurrentContext(), Nan::Undefined(), 2, arguments);
}

void JSTransfer::callback_failed(std::string error) {
	if(this->_self_ref) {
		this->_self_ref = false;
		this->Unref();
	}
	auto callback = Nan::Get(this->handle(), Nan::New<v8::String>("callback_failed").ToLocalChecked()).ToLocalChecked().As<v8::Function>();
	if(callback.IsEmpty() || !callback->IsFunction())
		return;

	v8::Local<v8::Value> arguments[1];
	arguments[0] = Nan::New<v8::String>(std::move(error)).ToLocalChecked();
    (void) callback->Call(Nan::GetCurrentContext(), Nan::Undefined(), 1, arguments);
}
#endif