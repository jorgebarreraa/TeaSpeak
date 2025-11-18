#pragma once

#include <fstream>
#include <optional>
#include "FileTransferManager.h"

namespace tc::ft {
    class TransferFileSource : public TransferSource {
        public:
            TransferFileSource(std::string /* path */, std::string /* name */);

            [[nodiscard]] inline std::string file_path() const { return this->_path; }
            [[nodiscard]] inline std::string file_name() const { return this->_name; }

            std::string name() const override { return "TransferFileSource"; }
            bool initialize(std::string &string) override;
            void finalize(bool) override;

            uint64_t byte_length() const override;
            uint64_t stream_index() const override;
            error::value read_bytes(std::string &string, uint8_t *uint8, uint64_t &uint64) override;

#ifdef NODEJS_API
            static NAN_METHOD(create);
#endif
        private:
            std::string _path;
            std::string _name;

            uint64_t position{0};
            std::ifstream file_stream{};
            mutable std::optional<size_t> file_size;
    };

    class TransferFileTarget : public TransferTarget {
        public:
            TransferFileTarget(std::string /* path */, std::string /* name */, size_t /* target size */);
            virtual ~TransferFileTarget();

            [[nodiscard]] std::string name() const override { return "TransferFileTarget"; }
            bool initialize(std::string &string) override;

            void finalize(bool) override;

            [[nodiscard]] uint64_t stream_index() const override { return this->position; }
            [[nodiscard]] uint64_t expected_length() const override { return this->target_file_size; }

            error::value write_bytes(std::string &string, uint8_t *uint8, uint64_t uint64) override;

#ifdef NODEJS_API
            static NAN_METHOD(create);
#endif
        private:
            std::string path_;
            std::string name_;

            uint64_t position{0};
            std::ofstream file_stream{};
            size_t target_file_size{};
    };

#ifdef NODEJS_API
    class TransferObjectWrap : public Nan::ObjectWrap {
        public:
            static NAN_MODULE_INIT(Init);
            static NAN_METHOD(NewInstance);

            static inline bool is_wrap(const v8::Local<v8::Value>& value) {
                if(value.As<v8::Object>().IsEmpty())
                    return false;

                return value->InstanceOf(Nan::GetCurrentContext(), Nan::New<v8::Function>(constructor())).FromMaybe(false);
            }

            static inline Nan::Persistent<v8::Function> & constructor() {
                static Nan::Persistent<v8::Function> my_constructor;
                return my_constructor;
            }

            explicit TransferObjectWrap(std::shared_ptr<TransferObject> object) :  _transfer(std::move(object)) {
            }

            ~TransferObjectWrap() override = default;

            void do_wrap(v8::Local<v8::Object> object);

            std::shared_ptr<TransferObject> target() { return this->_transfer; }
        private:
            std::shared_ptr<TransferObject> _transfer;
    };

    class TransferJSBufferSource : public TransferSource {
        public:
            TransferJSBufferSource();
            virtual ~TransferJSBufferSource();

            [[nodiscard]] std::string name() const override { return "TransferJSBufferSource"; }
            bool initialize(std::string &string) override;

            void finalize(bool) override;

            [[nodiscard]] uint64_t stream_index() const override;

            [[nodiscard]] uint64_t byte_length() const override;

            error::value read_bytes(std::string &string, uint8_t *uint8, uint64_t &uint64) override;

            static NAN_METHOD(create_from_buffer);

        private:
            v8::Global<v8::ArrayBuffer> _js_buffer;
            void* _js_buffer_source;
            uint64_t _js_buffer_length;
            uint64_t _js_buffer_index;
    };

    class TransferJSBufferTarget : public TransferTarget {
        public:
            TransferJSBufferTarget();
            virtual ~TransferJSBufferTarget();

            [[nodiscard]] std::string name() const override { return "TransferJSBufferTarget"; }
            bool initialize(std::string &string) override;

            void finalize(bool) override;

            [[nodiscard]] uint64_t stream_index() const override;
            [[nodiscard]] uint64_t expected_length() const override { return this->_js_buffer_length; }

            error::value write_bytes(std::string &string, uint8_t *uint8, uint64_t uint64) override;

            static NAN_METHOD(create_from_buffer);
        private:
            v8::Global<v8::ArrayBuffer> _js_buffer;
            void* _js_buffer_source;
            uint64_t _js_buffer_length;
            uint64_t _js_buffer_index;
    };
#endif
}