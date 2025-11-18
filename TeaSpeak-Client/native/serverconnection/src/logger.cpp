#include <iostream>
#include "logger.h"

/* Basic */
void(*logger::_force_log)(logger::category::value, logger::level::level_enum /* lvl */, const std::string_view& /* message */);

void force_log_raw(logger::category::value, spdlog::level::level_enum level, const std::string_view &message);
void force_log_node(logger::category::value, spdlog::level::level_enum, const std::string_view &);

#ifdef NODEJS_API
#include <NanGet.h>
#include <include/NanEventCallback.h>

/* NODE JS */
struct LogMessage {
	uint8_t level;
	uint8_t category;

	std::string message;

	LogMessage* next_message;
};
std::mutex log_messages_lock;
LogMessage* log_messages_head = nullptr;
LogMessage** log_messages_tail = &log_messages_head;

Nan::callback_t<> log_messages_callback;

struct StdExternalStringResourceBase : public v8::String::ExternalOneByteStringResource {
	public:
		explicit StdExternalStringResourceBase(const std::string& message) : message(message) {}

		const char *data() const override {
			return this->message.data();
		}

		size_t length() const override {
			return this->message.length();
		}

	private:
		std::string message;
};

inline v8::MaybeLocal<v8::Function> get_logger_method() {
    v8::Local<v8::Object> global_context = Nan::GetCurrentContext()->Global();

    auto logger = Nan::GetLocal<v8::Object>(global_context, "logger");
    if(!logger.IsEmpty()) {
	    auto log_function = Nan::Get<v8::Function>(logger, "log");
	    if(!log_function.IsEmpty()) return log_function;
    }

	auto console = Nan::GetLocal<v8::Object>(global_context, "console");
    assert(!console.IsEmpty());

    return Nan::Get<v8::Function>(console, "log");
}

void logger::initialize_node() {
	log_messages_callback = Nan::async_callback([]{
		Nan::HandleScope scope;

		auto isolate = Nan::GetCurrentContext()->GetIsolate();
		auto logger_method = get_logger_method();

		v8::Local<v8::Value> arguments[3];
		while(true) {
			std::unique_lock messages_lock(log_messages_lock);
			if(!log_messages_head)
				break;

			auto entry = log_messages_head;
			log_messages_head = entry->next_message;
			if(!log_messages_head)
				log_messages_tail = &log_messages_head;
			messages_lock.unlock();

			if(!logger_method.IsEmpty()) {
				auto logger = logger_method.ToLocalChecked();
				arguments[0] = Nan::New<v8::Number>(entry->category);
				arguments[1] = Nan::New<v8::Number>(entry->level);
				arguments[2] = v8::String::NewExternalOneByte(isolate, new StdExternalStringResourceBase(entry->message)).ToLocalChecked();

				logger.As<v8::Function>()->Call(Nan::GetCurrentContext(), Nan::Undefined(), 3, arguments);
			} else {
				std::cout << "Failed to log message! Invalid method!" << std::endl;
			}

			delete entry;
		}
	});

	logger::_force_log = force_log_node;
}

void force_log_node(logger::category::value category, spdlog::level::level_enum level, const std::string_view &message) {
	auto entry = new LogMessage{};
	entry->level = level;
	entry->category = category;
	entry->message = std::string(message.data(), message.length());
	entry->next_message = nullptr;

	{
		std::lock_guard lock(log_messages_lock);
		*log_messages_tail = entry;
		log_messages_tail = &(entry->next_message);
	}
	log_messages_callback();
}
#endif

void logger::initialize_raw() {
	logger::_force_log = force_log_raw;
}

void force_log_raw(logger::category::value category, spdlog::level::level_enum level, const std::string_view &message) {
	std::cout << "[" << level << "][" << category << "] " << message << std::endl;
}

void logger::err_handler(const std::string &message) {
    std::cout << "[ERROR] " << message << std::endl;
}