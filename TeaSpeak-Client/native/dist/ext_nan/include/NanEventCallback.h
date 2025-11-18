#pragma once

#include <functional>
#include <tuple>
#include <memory>
#include <mutex>
#include <uv.h>
#include <nan.h>

namespace Nan {
	namespace async_helper {
		template <typename...>
		struct callback_wrap;

		template <typename...>
		struct lambda_type;

		template<typename lambda, class ret, class klass, class... Args>
		struct lambda_type<lambda, ret(klass::*)(Args...) const > {

			using void_function = std::function<void(Args...)>;
			using result = callback_wrap<Args...>;

			inline static void_function wrap(const std::shared_ptr<lambda>& lam) {
				return [lam](Args&&... args) { (*lam)(std::forward<Args>(args)...); };
			}
		};

		template<typename lambda, class ret, class klass, class... Args>
		struct lambda_type<lambda, ret(klass::*)(Args...) > {
			using void_function = std::function<void(Args...)>;
			using result = callback_wrap<Args...>;

			inline static void_function wrap(const std::shared_ptr<lambda>& lam) {
				return [lam](Args&&... args) { (*lam)(std::forward<Args>(args)...); };
			}
		};

		// helper class
		template<typename R, template<typename...> class Params, typename... Args, std::size_t... I>
		inline  R call_helper(std::function<R(Args...)> const& func, Params<Args...> const& params, std::index_sequence<I...>) {
			return func(std::forward<Args>((Args&&) std::get<I>(params))...);
		}

		template <typename... Args>
		struct callback_args {
			uv_async_t handle{};
			bool flag_execute = false;
			bool destroy_only = false;
			bool option_destroy_run = false; /* execute the callback, even if the haven had been destoryed */
			std::mutex destroy_lock;

			std::function<void(Args...)> callback;
			std::tuple<Args...> arguments;
		};

		template <bool...>
		struct _or_ {
			constexpr static bool value = false;
		};

		template <bool T, bool... Args>
		struct _or_<T, Args...> {
			constexpr static bool value = T || _or_<Args...>::value;
		};


		template <typename... Args>
		struct callback_scoped {
			callback_args<Args...>* handle;
			std::function<void(Args&&...)> callback;
			std::function<void()> destroy;

			callback_scoped(
					callback_args<Args...>* handle,
					const std::function<void(Args &&...)> &callback,
					std::function<void()> destroy
			) : callback(callback), destroy(std::move(destroy)), handle(handle) {}

			~callback_scoped() { destroy(); }
		};

		template <typename... Args>
		struct callback_wrap {
			std::shared_ptr<callback_scoped<Args...>> handle;

			void call_cpy(Args... args, bool no_throw = false) const {
				if(!this->handle) {
					if(no_throw)
						return;
					throw std::bad_function_call();
				}
				handle->callback(std::forward<Args>(args)...);
			}

			void call(Args&&... args, bool no_throw = false) const {
				if(!this->handle) {
					if(no_throw)
						return;
					throw std::bad_function_call();
				}
				handle->callback(std::forward<Args>(args)...);
			}

			void operator()(Args&&... args) const {
				if(!this->handle)
					throw std::bad_function_call();
				handle->callback(std::forward<Args>(args)...);
			}

			callback_wrap<Args...>& operator=(const callback_wrap<Args...>& other) {
				this->handle = other.handle;

				return *this;
			}

			operator bool() const {
				return this->handle != nullptr;
			}

			callback_wrap<Args...>& operator=(nullptr_t) {
				this->handle = nullptr;
				return *this;
			}

			/**
			 * @returns true if the callback will be called even
			 * 			if the handle has been destroyed
			 */
			[[nodiscard]] bool option_destroyed_execute() const {
				if(!this->handle)
					std::__throw_logic_error("missing handle");
				return this->handle->handle->option_destroy_run;
			}

			/**
			 * @param flag
			 * 			If true then the callback will be called event
			 * 			if the handle has already been destroyed
			 * @return this
			 */
			callback_wrap<Args...>& option_destroyed_execute(bool flag) {
				if(!this->handle)
					throw std::logic_error("missing handle");
				this->handle->handle->option_destroy_run = flag;
				return *this;
			}
		};
	};
	template <typename... Args>
	using callback_t = async_helper::callback_wrap<Args...>;

	template <typename... Args, bool referenced_args = async_helper::_or_<std::is_reference<Args>::value...>::value>
	inline async_helper::callback_wrap<Args...> async_callback(std::function<void(Args...)> callback) {
		static_assert(!referenced_args, "Argument references aren't allowed");

		using callback_t = std::function<void(Args...)>;
		using callback_args = async_helper::callback_args<Args...>;

		auto args = new callback_args{};
		args->callback = std::move(callback);
		args->destroy_only = false;
		memset(&args->handle, 0, sizeof(args->handle));
		args->handle.data = args;

		uv_async_init(Nan::GetCurrentEventLoop(), &args->handle, [](uv_async_t* async) {
			auto _args = (callback_args*) async->data;
			std::unique_lock destroy_lock(_args->destroy_lock); //This may changed after calling the function (deleted while invoking is an example)
			if(!_args->destroy_only || (_args->option_destroy_run && _args->flag_execute)) {
				_args->flag_execute = false;
				auto arguments = std::move(_args->arguments); //Move the tuple now before it get overridden :)
				destroy_lock.unlock();
				async_helper::call_helper(_args->callback, arguments, std::index_sequence_for<Args...>{});
				destroy_lock.lock();
			}

			if(_args->destroy_only) {
				uv_close((uv_handle_t*) (void*) async, [](uv_handle_t* handle) {
					delete (callback_args*) handle->data;
				});
			}

		});

		return {
				std::make_shared<async_helper::callback_scoped<Args...>>(
						args,
						[args](Args&&... invoker_args) {
							std::lock_guard lock(args->destroy_lock);
							args->flag_execute = true;
							args->arguments = std::tuple<Args...>{std::forward<Args>(invoker_args)...};
							uv_async_send(&args->handle);
						},
						[args]{
							std::lock_guard lock(args->destroy_lock);
							args->destroy_only = true;
							uv_async_send(&args->handle);
						}
				)
		};
	}

	/* lambda edition */
	template <typename lambda,
			typename lambda_info = typename async_helper::lambda_type<lambda, decltype(&lambda::operator())>,
			typename result = typename lambda_info::result>
	inline result async_callback(lambda&& lam) {
		auto handle = std::make_shared<lambda>(std::forward<lambda>(lam));
		return async_callback(lambda_info::wrap(handle));
	}

	struct JavaScriptQueue {
        public:
            explicit JavaScriptQueue() {
                auto event_loop = Nan::GetCurrentEventLoop();
                assert(event_loop);

                this->callback_data = new CallbackData{};
                uv_async_init(event_loop, &this->callback_data->handle, JavaScriptQueue::async_send_callback);
            }

            ~JavaScriptQueue() {
                {
                    std::lock_guard lock{this->callback_data->callback_mutex};
                    this->callback_data->destroy = true;
                }
                uv_async_send(&this->callback_data->handle);
            }

            JavaScriptQueue(const JavaScriptQueue&) = delete;
            JavaScriptQueue(JavaScriptQueue&&) = delete;

            template <typename lambda>
            inline void enqueue(lambda&& callback) const {
                auto callable = new Callable<lambda>(std::forward<lambda>(callback));

                {
                    std::lock_guard lock{callback_data->callback_mutex};
                    *this->callback_data->callback_tail = callable;
                    this->callback_data->callback_tail = &callable->next;
                }

                uv_async_send(&this->callback_data->handle);
            }
        private:
            struct AbstractCallable;
            struct CallbackData {
                uv_async_t handle;
                bool destroy{false};

                std::mutex callback_mutex{};
                AbstractCallable* callback_head{nullptr};
                AbstractCallable** callback_tail{&this->callback_head};

                explicit CallbackData() {
                    memset(&this->handle, 0, sizeof(this->handle));
                    this->handle.data = this;
                }
            };

            struct AbstractCallable {
                AbstractCallable() = default;
                virtual ~AbstractCallable() = default;
                virtual void call() = 0;

                AbstractCallable* next{nullptr};
            };

            template <typename lambda>
            struct Callable : public AbstractCallable {
                explicit Callable(lambda&& callback) : callback{std::forward<lambda>(callback)} {}
                ~Callable() override = default;

                void call() override {
                    this->callback();
                }

                lambda callback;
            };

            static void async_send_callback(uv_async_t* handle) {
                auto data = (CallbackData*) handle->data;
                assert(data->handle.data == data);

                std::unique_lock lock{data->callback_mutex};
                auto destroy = data->destroy;
                auto callback_head = std::exchange(data->callback_head, nullptr);
                data->callback_tail = &data->callback_head;
                lock.unlock();

                Nan::HandleScope scope{};
                while(callback_head) {
                    callback_head->call();

                    auto next = callback_head->next;
                    delete callback_head;
                    callback_head = next;
                }

                if(destroy) {
                    /* uv_async_t inherits from uv_handle_t */
                    uv_close((uv_handle_t*) handle, JavaScriptQueue::async_close_callback);
                }
            }

            static void async_close_callback(uv_handle_t* handle) {
                delete (CallbackData*) handle->data;
            }

            CallbackData* callback_data;
    };
}