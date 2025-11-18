//
// Created by WolverinDEV on 18/03/2020.
//

#include <cassert>
#include "./SoundPlayer.h"
#include "../AudioOutput.h"
#include "../file/wav.h"
#include "../../EventLoop.h"
#include "../../logger.h"
#include "../AudioEventLoop.h"
#include "../AudioResampler.h"
#include "../AudioMerger.h"
#include "../AudioGain.h"

#ifdef NODEJS_API
#include <NanGet.h>
#include <NanEventCallback.h>
#endif

#ifdef max
    #undef max
#endif

using namespace tc::audio;

extern tc::audio::AudioOutput* global_audio_output;
namespace tc::audio::sounds {
    class FilePlayer : public event::EventEntry, public std::enable_shared_from_this<FilePlayer> {
        public:
            explicit FilePlayer(PlaybackSettings settings) : settings_{std::move(settings)} {
                log_trace(category::memory, tr("Allocated FilePlayer instance at {}"), (void*) this);
            }

            ~FilePlayer() {
                this->finalize(true);
                log_trace(category::memory, tr("Deleted FilePlayer instance at {}"), (void*) this);
            }

            [[nodiscard]] inline const PlaybackSettings& settings() const { return this->settings_; }

            [[nodiscard]] inline bool is_finished() const { return this->state_ == PLAYER_STATE_UNSET; }

            /* should not be blocking! */
            bool play() {
                if(this->state_ != PLAYER_STATE_UNSET) return false;
                this->state_ = PLAYER_STATE_INITIALIZE;

                audio::decode_event_loop->schedule(this->shared_from_this());
                return true;
            }

            /* should not be blocking! */
            void cancel() {
                this->state_ = PLAYER_STATE_CANCELED;
                audio::decode_event_loop->schedule(this->shared_from_this());
            }
        private:
            constexpr static auto kBufferChunkTimespan{0.2};

            const PlaybackSettings settings_;
            std::unique_ptr<file::WAVReader> file_handle{nullptr};
            std::unique_ptr<AudioResampler> resampler{nullptr};
            std::shared_ptr<audio::AudioOutputSource> output_source;

            void* cache_buffer{nullptr};

            enum {
                PLAYER_STATE_INITIALIZE,
                PLAYER_STATE_PLAYING,
                PLAYER_STATE_AWAIT_FINISH,
                PLAYER_STATE_FINISHED,
                PLAYER_STATE_CANCELED,
                PLAYER_STATE_UNSET
            } state_{PLAYER_STATE_UNSET};

            void finalize(bool is_destructor_call) {
                this->output_source = nullptr;
                if(this->file_handle)
                    this->file_handle = nullptr;
                if(auto buffer{std::exchange(this->cache_buffer, nullptr)}; buffer)
                    ::free(buffer);
                if(!is_destructor_call)
                    audio::decode_event_loop->cancel(this->shared_from_this());
                this->state_ = PLAYER_STATE_UNSET;
            }

            void event_execute(const std::chrono::system_clock::time_point &) override {
                if(this->state_ == PLAYER_STATE_INITIALIZE) {
                    this->file_handle = std::make_unique<file::WAVReader>(this->settings_.file);
                    std::string error{};
                    if(auto err{this->file_handle->open_file(error)}; err != file::OPEN_RESULT_SUCCESS) {
                        if(auto callback{this->settings_.callback}; callback) {
                            callback(PlaybackResult::FILE_OPEN_ERROR, error);
                        }
                        this->finalize(false);
                        return;
                    }

                    if(!global_audio_output) {
                        if(auto callback{this->settings_.callback}; callback) {
                            callback(PlaybackResult::SOUND_NOT_INITIALIZED, "");
                        }
                        this->finalize(false);
                        return;
                    }

                    this->initialize_playback();

                    auto max_samples = (size_t)
                            std::max(this->output_source->sample_rate(), this->file_handle->sample_rate()) * kBufferChunkTimespan * 8 *
                                       std::max(this->file_handle->channels(), this->output_source->channel_count());

                    this->cache_buffer = ::malloc((size_t) (max_samples * sizeof(float)));

                    if(!this->cache_buffer) {
                        if(auto callback{this->settings_.callback}; callback) {
                            callback(PlaybackResult::PLAYBACK_ERROR, "failed to allocate cached buffer");
                        }

                        this->finalize(false);
                        return;
                    }
                    this->state_ = PLAYER_STATE_PLAYING;
                }

                if(this->state_ == PLAYER_STATE_PLAYING) {
                    if(!this->could_enqueue_next_buffer()) {
                        return;
                    }

                    auto samples_to_read = (size_t) (this->file_handle->sample_rate() * kBufferChunkTimespan);
                    auto errc = this->file_handle->read(this->cache_buffer, &samples_to_read);
                    switch (errc) {
                        case file::READ_RESULT_SUCCESS:
                            break;

                        case file::READ_RESULT_EOF:
                            this->state_ = PLAYER_STATE_AWAIT_FINISH;
                            return;


                        case file::READ_RESULT_UNRECOVERABLE_ERROR:
                            if(auto callback{this->settings_.callback}; callback)
                                callback(PlaybackResult::PLAYBACK_ERROR, "read resulted in an unrecoverable error");

                            this->finalize(false);
                            return;
                    }

                    if(!merge::merge_channels_interleaved(this->cache_buffer, this->output_source->channel_count(), this->cache_buffer, this->file_handle->channels(), samples_to_read)) {
                        log_warn(category::audio, tr("failed to merge channels for replaying a sound"));
                        return;
                    }

                    size_t resampled_samples{this->cache_buffer_sample_size()};
                    if(!this->resampler->process(this->cache_buffer, this->cache_buffer, samples_to_read, resampled_samples)) {
                        log_warn(category::audio, tr("failed to resample file audio buffer."));
                        return;
                    }

                    audio::apply_gain(this->cache_buffer, this->output_source->channel_count(), resampled_samples, this->settings_.volume);
                    this->output_source->enqueue_samples(this->cache_buffer, resampled_samples);
                    if(this->could_enqueue_next_buffer())
                        audio::decode_event_loop->schedule(this->shared_from_this());
                } else if(this->state_ == PLAYER_STATE_FINISHED || this->state_ == PLAYER_STATE_CANCELED) {
                    this->finalize(false);
                    if(auto callback{this->settings_.callback}; callback)
                        callback(this->state_ == PLAYER_STATE_CANCELED ? PlaybackResult::CANCELED : PlaybackResult::SUCCEEDED, "");
                    this->state_ = PLAYER_STATE_UNSET;
                    return;
                }
                auto filled_samples = this->output_source->currently_buffered_samples();
            }

            void initialize_playback() {
                assert(this->file_handle);
                assert(global_audio_output);

                const auto max_buffer = (size_t) ceil(global_audio_output->sample_rate() * kBufferChunkTimespan * 3);
                this->output_source = global_audio_output->create_source(max_buffer);
                this->output_source->overflow_strategy = audio::OverflowStrategy::ignore;
                this->output_source->set_max_buffered_samples(max_buffer);
                this->output_source->set_min_buffered_samples((size_t) floor(this->output_source->sample_rate() * 0.04));

                auto weak_this = this->weak_from_this();
                this->output_source->on_underflow = [weak_this](size_t sample_count){
                    auto self = weak_this.lock();
                    if(!self) {
                        return false;
                    }

                    /* The execute lock must be locked else some internal state could be altered */
                    auto execute_lock = self->execute_lock(true);
                    if(self->state_ == PLAYER_STATE_PLAYING) {
                        log_warn(category::audio, tr("Having an audio underflow while playing a sound."));
                    } else if(self->state_ == PLAYER_STATE_AWAIT_FINISH) {
                        self->state_ = PLAYER_STATE_FINISHED;
                    }
                    audio::decode_event_loop->schedule(self);
                    return false;
                };
                this->output_source->on_read = [weak_this] {
                    auto self = weak_this.lock();
                    if(!self) {
                        return;
                    }

                    /* The execute lock must be locked else some internal state could be altered */
                    auto execute_lock = self->execute_lock(true);
                    if(self->could_enqueue_next_buffer() && self->state_ == PLAYER_STATE_PLAYING) {
                        audio::decode_event_loop->schedule(self);
                    }
                };

                this->output_source->on_overflow = [weak_this](size_t count) {
                    log_warn(category::audio, tr("Having an audio overflow while playing a sound."));
                };

                this->resampler = std::make_unique<AudioResampler>(this->file_handle->sample_rate(), this->output_source->sample_rate(), this->output_source->channel_count());
            }


            [[nodiscard]] inline size_t cache_buffer_sample_size() const {
                return (size_t) (this->output_source->sample_rate() * kBufferChunkTimespan);
            }

            [[nodiscard]] inline bool could_enqueue_next_buffer() const {
                if(!this->output_source) {
                    return false;
                }

                const auto current_size = this->output_source->currently_buffered_samples();
                const auto max_size = this->output_source->max_buffered_samples();
                if(current_size > max_size) {
                    return false;
                }

                const auto size_left = max_size - current_size;
                return size_left >= this->cache_buffer_sample_size() * 1.5; /* ensure we've a bit more space */
            }
    };

    std::mutex file_player_mutex{};
    std::deque<std::shared_ptr<FilePlayer>> file_players{};

    sound_playback_id playback_sound(const PlaybackSettings& settings) {
        if(!audio::initialized()) {
            settings.callback(PlaybackResult::SOUND_NOT_INITIALIZED, "");
            return 0;
        }

        std::unique_lock fplock{file_player_mutex};
        file_players.erase(std::remove_if(file_players.begin(), file_players.end(), [](const auto& player) {
            return player->is_finished();
        }), file_players.end());

        auto player = std::make_shared<FilePlayer>(settings);
        file_players.push_back(player);
        if(!player->play()) {
            if(auto callback{settings.callback}; callback) {
                callback(PlaybackResult::PLAYBACK_ERROR, "failed to start playback.");
            }
            return 0;
        }
        fplock.unlock();
        return (sound_playback_id) &*player;
    }

    void cancel_playback(const sound_playback_id& id) {
        std::unique_lock fplock{file_player_mutex};
        auto player_it = std::find_if(file_players.begin(), file_players.end(), [&](const auto& player) { return (sound_playback_id) &*player == id; });
        if(player_it == file_players.end()) {
            return;
        }

        auto player = *player_it;
        file_players.erase(player_it);
        fplock.unlock();

        player->cancel();
    }
}

#ifdef NODEJS_API
NAN_METHOD(tc::audio::sounds::playback_sound_js) {
    if(info.Length() != 1 || !info[0]->IsObject()) {
        Nan::ThrowError("invalid arguments");
        return;
    }

    auto data = info[0].As<v8::Object>();
    auto file = Nan::GetLocal<v8::String>(data, "file");
    auto volume = Nan::GetLocal<v8::Number>(data, "volume", Nan::New<v8::Number>(1.f));
    v8::Local<v8::Function> callback = Nan::GetLocal<v8::Function>(data, "callback");
    if(file.IsEmpty() || !file->IsString()) {
        Nan::ThrowError("missing file path");
        return;
    }
    if(volume.IsEmpty() || !volume->IsNumber()) {
        Nan::ThrowError("invalid volume");
        return;
    }

    PlaybackSettings settings{};
    settings.file = *Nan::Utf8String(file);
    settings.volume = (float) volume->Value();
    if(!callback.IsEmpty()) {
        if(!callback->IsFunction()) {
            Nan::ThrowError("invalid callback function");
            return;
        }

        Nan::Global<v8::Function> cb{callback};
        auto async_callback = Nan::async_callback([cb = std::move(cb)](PlaybackResult result, std::string error) mutable {
            Nan::HandleScope scope{};
            auto callback = cb.Get(Nan::GetCurrentContext()->GetIsolate()).As<v8::Function>();
            cb.Reset();

            v8::Local<v8::Value> arguments[2];
            arguments[0] = Nan::New<v8::Number>((int) result);
            arguments[1] = Nan::LocalStringUTF8(error);
            (void) callback->Call(Nan::GetCurrentContext(), Nan::Undefined(), 2, arguments);
        }).option_destroyed_execute(true);

        settings.callback = [async_callback](PlaybackResult result, const std::string& error) mutable {
            async_callback.call_cpy(result, error);
        };
    }

    info.GetReturnValue().Set((uint32_t) playback_sound(settings));
}

NAN_METHOD(tc::audio::sounds::cancel_playback_js) {
    if(info.Length() != 1 || !info[0]->IsNumber()) {
        Nan::ThrowError("invalid arguments");
        return;
    }

    cancel_playback((sound_playback_id) info[0].As<v8::Number>()->Value());
}
#endif