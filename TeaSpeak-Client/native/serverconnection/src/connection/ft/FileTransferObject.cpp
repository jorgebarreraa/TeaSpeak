#include "FileTransferObject.h"
#include "../../logger.h"
#include <iostream>

#ifdef WIN32
    #include <filesystem>
    namespace fs = std::filesystem;
#else
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#endif

using namespace tc;
using namespace tc::ft;
using namespace std;

#ifdef NODEJS_API
TransferJSBufferTarget::TransferJSBufferTarget() {
	log_allocate("TransferJSBufferTarget", this);

	if(!this->_js_buffer.IsEmpty()) {
		assert(v8::Isolate::GetCurrent());
		this->_js_buffer.Reset();
	}
}

TransferJSBufferTarget::~TransferJSBufferTarget() {
	log_free("TransferJSBufferTarget", this);
}

bool TransferJSBufferTarget::initialize(std::string &error) {
	return true; /* we've already have data */
}

void TransferJSBufferTarget::finalize(bool) { }

uint64_t TransferJSBufferTarget::stream_index() const {
	return this->_js_buffer_index;
}

error::value TransferJSBufferTarget::write_bytes(std::string &error, uint8_t *source, uint64_t length) {
	uint64_t write_length = length;
	if(length > this->_js_buffer_length - this->_js_buffer_index)
		write_length = this->_js_buffer_length - this->_js_buffer_index;

	if(write_length > 0) {
		memcpy((char*) this->_js_buffer_source + this->_js_buffer_index, source, write_length);
		this->_js_buffer_index += write_length;
	}

	if(write_length == 0)
		return error::out_of_space;

	return error::success;
}

NAN_METHOD(TransferJSBufferTarget::create_from_buffer) {
	if(info.Length() != 1 || !info[0]->IsArrayBuffer()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	auto buffer = info[0].As<v8::ArrayBuffer>();

	auto instance = make_shared<TransferJSBufferTarget>();
	instance->_js_buffer = v8::Global<v8::ArrayBuffer>(info.GetIsolate(), info[0].As<v8::ArrayBuffer>());

	instance->_js_buffer_source = buffer->GetContents().Data();
	instance->_js_buffer_length = buffer->GetContents().ByteLength();
	instance->_js_buffer_index = 0;


	auto object_wrap = new TransferObjectWrap(instance);
	auto object = Nan::NewInstance(Nan::New(TransferObjectWrap::constructor()), 0, nullptr).ToLocalChecked();
	object_wrap->do_wrap(object);
	info.GetReturnValue().Set(object);
}

TransferJSBufferSource::~TransferJSBufferSource() {
	log_free("TransferJSBufferSource", this);

	if(!this->_js_buffer.IsEmpty()) {
		assert(v8::Isolate::GetCurrent());
		this->_js_buffer.Reset();
	}
}
TransferJSBufferSource::TransferJSBufferSource() {
	log_allocate("TransferJSBufferSource", this);
}

bool TransferJSBufferSource::initialize(std::string &string) { return true; }

void TransferJSBufferSource::finalize(bool) { }

uint64_t TransferJSBufferSource::stream_index() const {
	return this->_js_buffer_index;
}

uint64_t TransferJSBufferSource::byte_length() const {
	return this->_js_buffer_length;
}

error::value TransferJSBufferSource::read_bytes(std::string &error, uint8_t *target, uint64_t &length) {
	auto org_length = length;
	if(this->_js_buffer_index + length > this->_js_buffer_length)
		length = this->_js_buffer_length - this->_js_buffer_index;


	memcpy(target, (char*) this->_js_buffer_source + this->_js_buffer_index, length);
	this->_js_buffer_index += length;

	if(org_length != 0 && length == 0)
		return error::out_of_space;
	return error::success;
}

NAN_METHOD(TransferJSBufferSource::create_from_buffer) {
	if(info.Length() != 1 || !info[0]->IsArrayBuffer()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	auto buffer = info[0].As<v8::ArrayBuffer>();

	auto instance = make_shared<TransferJSBufferSource>();
	instance->_js_buffer = v8::Global<v8::ArrayBuffer>(info.GetIsolate(), info[0].As<v8::ArrayBuffer>());

	instance->_js_buffer_source = buffer->GetContents().Data();
	instance->_js_buffer_length = buffer->GetContents().ByteLength();
	instance->_js_buffer_index = 0;


	auto object_wrap = new TransferObjectWrap(instance);
	auto object = Nan::NewInstance(Nan::New(TransferObjectWrap::constructor()), 0, nullptr).ToLocalChecked();
	object_wrap->do_wrap(object);
	info.GetReturnValue().Set(object);
}


NAN_MODULE_INIT(TransferObjectWrap::Init) {
	auto klass = Nan::New<v8::FunctionTemplate>(TransferObjectWrap::NewInstance);
	klass->SetClassName(Nan::New("TransferObjectWrap").ToLocalChecked());
	klass->InstanceTemplate()->SetInternalFieldCount(1);

	constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(TransferObjectWrap::NewInstance) {
	if(!info.IsConstructCall())
		Nan::ThrowError("invalid invoke!");
}

void TransferObjectWrap::do_wrap(v8::Local<v8::Object> object) {
	this->Wrap(object);

	auto source = dynamic_pointer_cast<TransferSource>(this->target());
	auto target = dynamic_pointer_cast<TransferTarget>(this->target());

	auto direction = source ? "upload" : "download";
	Nan::Set(object,
	         Nan::New<v8::String>("direction").ToLocalChecked(),
	         Nan::New<v8::String>(direction).ToLocalChecked()
	);
	Nan::Set(object,
	         Nan::New<v8::String>("name").ToLocalChecked(),
             Nan::New<v8::String>(this->target()->name().c_str()).ToLocalChecked()
	);

	if(source) {
		Nan::Set(object,
		         Nan::New<v8::String>("total_size").ToLocalChecked(),
		         Nan::New<v8::Number>((double) source->byte_length())
		);
	}

	if(target) {
		Nan::Set(object,
		         Nan::New<v8::String>("expected_size").ToLocalChecked(),
		         Nan::New<v8::Number>((double) target->expected_length())
		);

	}
}
#endif

TransferFileSource::TransferFileSource(std::string path, std::string name) : _path{std::move(path)}, _name{std::move(name)} {
	if(!this->_path.empty()) {
		if(this->_path.back() == '/')
			this->_path.pop_back();
#ifdef WIN32
		if(this->_path.back() == '\\')
			this->_path.pop_back();
#endif
	}
}

uint64_t TransferFileSource::byte_length() const {
	if(file_size.has_value())
		return file_size.value();

	auto file = fs::u8path(this->_path) / fs::u8path(this->_name);
	error_code error;
	auto size = fs::file_size(file,error);
	if(error)
		size = 0;
	return (this->file_size = std::make_optional<size_t>(size)).value();
}

bool TransferFileSource::initialize(std::string &error) {
	auto file = fs::u8path(this->_path) / fs::u8path(this->_name);
    log_debug(category::file_transfer, tr("Opening source file for transfer: {} ({})"), file.string(), fs::absolute(file).string());

	error_code errc;
	if(!fs::exists(file)) {
		error = "file not found";
		return false;
	} else if(errc) {
		error = "failed to test for file existence: " + to_string(errc.value()) + "/" + errc.message();
		return false;
	}
	if(!fs::is_regular_file(file, errc)) {
		error = "target file isn't a regular file";
		return false;
	}
	if(errc) {
		error = "failed to test for file regularity: " + to_string(errc.value()) + "/" + errc.message();
		return false;
	}

	this->file_stream = std::ifstream{file, std::ifstream::in | std::ifstream::binary};
	if(!this->file_stream) {
		error = "failed to open file";
		return false;
	}

	this->file_stream.seekg(0, std::ifstream::end);
	auto length = this->file_stream.tellg();
	if(length != this->byte_length()) {
		error = "file length missmatch";
		return false;
	}
	this->file_stream.seekg(0, std::ifstream::beg);

	this->position = 0;
	return true;
}

void TransferFileSource::finalize(bool) {
	if(this->file_stream)
		this->file_stream.close();

	this->position = 0;
}

error::value TransferFileSource::read_bytes(std::string &error, uint8_t *buffer, uint64_t &length) {
    error.clear();
#ifdef WIN32
    auto blength = this->byte_length();
    if(this->position >= blength) {
        error = "eof reached";
        return error::custom;
    }
    if(this->position + length > this->byte_length())
        length = this->byte_length() - this->position;
    this->file_stream.read((char*) buffer, length);
    this->position += length;
#else
    auto result = this->file_stream.readsome((char*) buffer, length);
	if(result > 0) {
		length = result;
		this->position += result;
		return error::success;
	} else if(result == 0) {
	    return error::would_block;
	} else
		error = "read returned " + to_string(result) + "/" + to_string(length);
#endif

	if(!this->file_stream) {
		if(this->file_stream.eof())
			error = "eof reached";
		else
			error = "io error. failed to read";
	}
	return error.empty() ? error::success : error::custom;
}

uint64_t TransferFileSource::stream_index() const {
	return this->position;
}

TransferFileTarget::TransferFileTarget(std::string path, std::string name, size_t target_size) : path_{std::move(path)}, name_{std::move(name)}, target_file_size{target_size} {
    if(!this->path_.empty()) {
        if(this->path_.back() == '/')
            this->path_.pop_back();
#ifdef WIN32
        if(this->path_.back() == '\\')
            this->path_.pop_back();
#endif
    }
}

TransferFileTarget::~TransferFileTarget() = default;

bool TransferFileTarget::initialize(std::string &error) {
    auto targetFile = fs::u8path(this->path_) / fs::u8path(this->name_);
    auto downloadFile = fs::u8path(this->path_) / fs::u8path(this->name_ + ".download");
    log_debug(category::file_transfer, tr("Opening target file for transfer: {}"), downloadFile.string());

    std::error_code fs_error;
    if(fs::exists(fs::u8path(this->path_), fs_error)) {
        if(fs::exists(downloadFile, fs_error)) {
            if(!fs::remove(downloadFile, fs_error) || fs_error)
                log_warn(category::file_transfer, tr("Failed to remove old temporary .download file for {}: {}/{}"), downloadFile.string(), fs_error.value(), fs_error.message());
        } else if(fs_error) {
            log_warn(category::file_transfer, tr("Failed to check for temp download file existence at {}: {}/{}"), downloadFile.string(), fs_error.value(), fs_error.message());
        }
    } else if(fs_error) {
        log_error(category::file_transfer, tr("Failed to check for directory existence at {}: {}/{}"), this->path_, fs_error.value(), fs_error.message());
        error = tr("failed to check for directory existence");
        return false;
    } else if(!fs::create_directories(fs::u8path(this->path_), fs_error) || fs_error) {
        error = tr("failed to create directories: ") + std::to_string(fs_error.value()) + "/" + fs_error.message();
        return false;
    }

    if(fs::exists(targetFile, fs_error)) {
        if(!fs::remove(targetFile, fs_error) || fs_error) {
            error = tr("failed to delete old file: ") + std::to_string(fs_error.value()) + "/" + fs_error.message();
            return false;
        }
    } else if(fs_error) {
        log_warn(category::file_transfer, tr("Failed to check for target file existence at {}: {}/{}. Assuming it does not exists."), targetFile.string(), fs_error.value(), fs_error.message());
    }

    this->file_stream = std::ofstream{downloadFile, std::ofstream::out | std::ofstream::binary};
    if(!this->file_stream) {
        error = tr("file to open file: ") + std::string{strerror(errno)};
        return false;
    }

    this->position = 0;
    return true;
}

void TransferFileTarget::finalize(bool aborted) {
    if(this->file_stream)
        this->file_stream.close();

    std::error_code fs_error{};
    auto downloadFile = fs::u8path(this->path_) / fs::u8path(this->name_ + ".download");
    if(aborted) {
        if(!fs::remove(downloadFile, fs_error) || fs_error)
            log_warn(category::file_transfer, tr("Failed to remove .download file from aborted transfer for {}: {}/{}."), downloadFile.string(), fs_error.value(), fs_error.message());
    } else {
        auto target = fs::u8path(this->path_) / fs::u8path(this->name_);

        if(this->file_stream)
            this->file_stream.close();

        fs::rename(downloadFile, target, fs_error);
        if(fs_error)
            log_warn(category::file_transfer, tr("Failed to rename file {} to {}: {}/{}"), downloadFile.string(), target.string(), fs_error.value(), fs_error.message());
    }

    this->position = 0;
}

error::value TransferFileTarget::write_bytes(std::string &error, uint8_t *buffer, uint64_t length) {
    this->file_stream.write((char*) buffer, length);
    this->position += length;

    if(!this->file_stream) {
        if(this->file_stream.eof())
            error = "eof reached";
        else
            error = "io error. failed to write";
    }
    return error.empty() ? error::success : error::custom;
}

#ifdef NODEJS_API
NAN_METHOD(TransferFileSource::create) {
	if(info.Length() != 2 || !info[0]->IsString() || !info[1]->IsString()) {
		Nan::ThrowError("invalid arguments");
		return;
	}

	auto instance = make_shared<TransferFileSource>(*Nan::Utf8String{info[0]}, *Nan::Utf8String{info[1]});
	auto object_wrap = new TransferObjectWrap(instance);
	auto object = Nan::NewInstance(Nan::New(TransferObjectWrap::constructor()), 0, nullptr).ToLocalChecked();
	object_wrap->do_wrap(object);
	info.GetReturnValue().Set(object);
}

NAN_METHOD(TransferFileTarget::create) {
	if(info.Length() != 3 || !info[0]->IsString() || !info[1]->IsString() || !info[2]->IsNumber()) {
		Nan::ThrowError("invalid arguments");
		return;
	}

	auto instance = make_shared<TransferFileTarget>(*Nan::Utf8String{info[0]}, *Nan::Utf8String{info[1]}, info[2]->IntegerValue(Nan::GetCurrentContext()).FromMaybe(0));
	auto object_wrap = new TransferObjectWrap(instance);
	auto object = Nan::NewInstance(Nan::New(TransferObjectWrap::constructor()), 0, nullptr).ToLocalChecked();
	object_wrap->do_wrap(object);
	info.GetReturnValue().Set(object);
}
#endif