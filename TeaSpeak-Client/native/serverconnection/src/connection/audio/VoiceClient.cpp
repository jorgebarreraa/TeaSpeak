#include "VoiceClient.h"
#include "../../audio/codec/OpusConverter.h"
#include "../../audio/AudioMerger.h"
#include "../../audio/js/AudioOutputStream.h"
#include "../../audio/AudioEventLoop.h"
#include "../../audio/AudioGain.h"

using namespace std;
using namespace tc;
using namespace tc::audio::codec;
using namespace tc::connection;

extern tc::audio::AudioOutput* global_audio_output;

#define DEBUG_PREMATURE_PACKETS

void VoiceClientWrap::do_wrap(const v8::Local<v8::Object> &object) {
	this->Wrap(object);

	auto handle = this->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}
	Nan::Set(object, Nan::New<v8::String>("client_id").ToLocalChecked(), Nan::New<v8::Number>(handle->client_id()));
	handle->on_state_changed = [&]{ this->call_state_changed(); };

	this->call_state_changed = Nan::async_callback([&]{
		Nan::HandleScope scope{};
		this->call_state_changed_();
	});
}

void VoiceClientWrap::call_state_changed_() {
	auto handle = this->_handle.lock();
	if(!handle) {
		log_warn(category::voice_connection, tr("State changed on invalid handle!"));
		return;
	}

	auto state = handle->state();

	const auto was_playing = this->currently_playing_;
	if(state == VoiceClient::state::stopped) {
		this->currently_playing_ = false;
	} else if(state == VoiceClient::state::playing) {
		this->currently_playing_ = true;
	}

	if(!was_playing && this->currently_playing_) {
		auto callback = Nan::Get(this->handle(), Nan::New<v8::String>("callback_playback").ToLocalChecked()).ToLocalChecked();
		if(callback->IsFunction()) {
			(void) callback.As<v8::Function>()->Call(Nan::GetCurrentContext(), Nan::Undefined(), 0, nullptr);
		}
	}
	if(was_playing && !this->currently_playing_) {
		auto callback = Nan::Get(this->handle(), Nan::New<v8::String>("callback_stopped").ToLocalChecked()).ToLocalChecked();
		if(callback->IsFunction()) {
			(void) callback.As<v8::Function>()->Call(Nan::GetCurrentContext(), Nan::Undefined(), 0, nullptr);
		}
	}

	auto callback = Nan::Get(this->handle(), Nan::New<v8::String>("callback_state_changed").ToLocalChecked()).ToLocalChecked();
	if(callback->IsFunction()) {
		v8::Local<v8::Value> argv[1] = {
			Nan::New<v8::Number>(state)
		};
		(void) callback.As<v8::Function>()->Call(Nan::GetCurrentContext(), Nan::Undefined(), 1, argv);
	}
}

NAN_MODULE_INIT(VoiceClientWrap::Init) {
	auto klass = Nan::New<v8::FunctionTemplate>(VoiceClientWrap::NewInstance);
	klass->SetClassName(Nan::New("VoiceConnection").ToLocalChecked());
	klass->InstanceTemplate()->SetInternalFieldCount(1);

	Nan::SetPrototypeMethod(klass, "get_state", VoiceClientWrap::_get_state);
	Nan::SetPrototypeMethod(klass, "get_volume", VoiceClientWrap::_get_volume);
	Nan::SetPrototypeMethod(klass, "set_volume", VoiceClientWrap::_set_volume);
	Nan::SetPrototypeMethod(klass, "abort_replay", VoiceClientWrap::_abort_replay);
	Nan::SetPrototypeMethod(klass, "get_stream", VoiceClientWrap::_get_stream);

	constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(VoiceClientWrap::NewInstance) {
	if(!info.IsConstructCall())
		Nan::ThrowError("invalid invoke!");
}

NAN_METHOD(VoiceClientWrap::_get_volume) {
	auto client = ObjectWrap::Unwrap<VoiceClientWrap>(info.Holder());

	auto handle = client->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}

	info.GetReturnValue().Set(handle->get_volume());
}

NAN_METHOD(VoiceClientWrap::_set_volume) {
	auto client = ObjectWrap::Unwrap<VoiceClientWrap>(info.Holder());

	auto handle = client->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}

	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("Invalid arguments");
		return;
	}

	handle->set_volume((float) info[0]->NumberValue(Nan::GetCurrentContext()).FromMaybe(0));
}
NAN_METHOD(VoiceClientWrap::_abort_replay) {
	auto client = ObjectWrap::Unwrap<VoiceClientWrap>(info.Holder());

	auto handle = client->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}

	handle->cancel_replay();
}

NAN_METHOD(VoiceClientWrap::_get_state) {
	auto client = ObjectWrap::Unwrap<VoiceClientWrap>(info.Holder());

	auto handle = client->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}

	info.GetReturnValue().Set(handle->state());
}


NAN_METHOD(VoiceClientWrap::_get_stream) {
	auto client = ObjectWrap::Unwrap<VoiceClientWrap>(info.Holder());

	auto handle = client->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}

	auto wrapper = new audio::AudioOutputStreamWrapper(handle->output_stream(), false);
	auto object = Nan::NewInstance(Nan::New(audio::AudioOutputStreamWrapper::constructor()), 0, nullptr).ToLocalChecked();
	wrapper->do_wrap(object);
	info.GetReturnValue().Set(object);
}

VoiceClientWrap::VoiceClientWrap(const std::shared_ptr<VoiceClient>& client) : _handle(client) { }

VoiceClientWrap::~VoiceClientWrap() = default;

VoiceClient::VoiceClient(const std::shared_ptr<VoiceConnection>&, uint16_t client_id) : client_id_(client_id) {
	this->execute_lock_timeout = std::chrono::microseconds{500};
}

VoiceClient::~VoiceClient() {
	if(v8::Isolate::GetCurrent()) {
		this->finalize_js_object();
	} else {
		assert(this->js_handle_.IsEmpty());
	}

	this->cancel_replay(); /* cleanup all buffers */
	if(this->output_source) {
        this->output_source->on_underflow = nullptr; /* to ensure */
        this->output_source = nullptr;
	}
}

void VoiceClient::initialize() {
    auto weak_this = this->ref_;

    audio::initialize([weak_this]{
        auto client = weak_this.lock();
        if(!client) {
            return;
        }

        assert(global_audio_output);
        client->output_source = global_audio_output->create_source();
        client->output_source->overflow_strategy = audio::OverflowStrategy::ignore;
        client->output_source->set_max_buffered_samples((size_t) ceil(client->output_source->sample_rate() * 0.5));
        client->output_source->set_min_buffered_samples((size_t) ceil(client->output_source->sample_rate() * 0.04));

        client->output_source->on_underflow = [weak_this](size_t sample_count) {
			auto client = weak_this.lock();
			if(!client) {
				return false;
			}

			return client->handle_output_underflow(sample_count);
        };

        client->output_source->on_overflow = [weak_this](size_t count){
			auto client = weak_this.lock();
			if(!client) {
				return;
			}

            log_warn(category::audio, tr("Client {} has a audio buffer overflow of {}."), client->client_id_, count);
        };
    });
}

void VoiceClient::execute_tick() {
	switch(this->state_) {
		case state::buffering:
			if(this->packet_queue.last_packet_timestamp + chrono::milliseconds{250} < chrono::system_clock::now()) {
				this->set_state(state::stopped);
				log_debug(category::audio, tr("Audio stop packet for client {} seems to be lost. Stopping playback."), this->client_id_);
			}
			break;

        case state::stopping: {
            auto output = this->output_source;
            if(!output) {
                this->set_state(state::stopped);
                break;
            }

            using BufferState = audio::AudioOutputSource::BufferState;
            switch(output->state()) {
				case BufferState::fadeout:
					/*
					 * Even though we're in fadeout it's pretty reasonable to already set the state to stopped
					 * especially since the tick method will only be called all 500ms.
					 */

				case BufferState::buffering:
					/* We have no more data to replay */

					this->set_state(state::stopped);
					break;

				case BufferState::playing:
					break;

            	default:
            		assert(false);
            		break;
            }
            break;
        }

		case state::playing:
		case state::stopped:
			/* Nothing to do or to check. */
			break;

		default:
			assert(false);
			break;
	}
}

void VoiceClient::initialize_js_object() {
	if(!this->js_handle_.IsEmpty())
		return;

	auto object_wrap = new VoiceClientWrap(this->ref());
	auto object = Nan::NewInstance(Nan::New(VoiceClientWrap::constructor()), 0, nullptr).ToLocalChecked();
	Nan::TryCatch tc{};
	object_wrap->do_wrap(object);
	if(tc.HasCaught()) {
		tc.ReThrow();
		return;
	}

	this->js_handle_.Reset(Nan::GetCurrentContext()->GetIsolate(), object);
}

void VoiceClient::finalize_js_object() {
	this->js_handle_.Reset();
}

/**
 * @param lower The packet ID which should be lower than the other
 * @param upper The packet id which should be higher than the lower one
 * @param clip_window The size how long the "overflow" counts
 * @return true if lower is less than upper
 */
inline constexpr bool packet_id_less(uint16_t lower, uint16_t upper, uint16_t window) {
	constexpr auto bounds = std::numeric_limits<uint16_t>::max();

	if(bounds - window <= lower) {
		uint16_t max_clip = lower + window;
		if(upper <= max_clip) {
			return true;
		} else if(upper > lower) {
			return true;
		} else {
            return false;
		}
	} else {
		if(lower >= upper) {
			return false;
		}

		return upper - lower <= window;
	}
}

inline constexpr uint16_t packet_id_diff(uint16_t lower, uint16_t upper) {
	if(upper < lower) {
        return (uint16_t) (((uint32_t) upper | 0x10000U) - (uint32_t) lower);
	}
	return upper - lower;
}

#define MAX_LOST_PACKETS (6)
void VoiceClient::process_packet(uint16_t packet_id, const pipes::buffer_view& buffer, uint8_t buffer_codec, bool is_head) {
#if 0
	if(rand() % 10 == 0) {
		log_info(category::audio, tr("Dropping audio packet id {}"), packet_id);
		return;
	}
#endif
	auto encoded_buffer = new EncodedBuffer{};
	encoded_buffer->packet_id = packet_id;
	encoded_buffer->codec = buffer_codec;
	encoded_buffer->receive_timestamp = chrono::system_clock::now();
	encoded_buffer->buffer = buffer.own_buffer();
	encoded_buffer->head = is_head;

	{
		lock_guard lock{this->packet_queue.pending_lock};
		if(this->packet_queue.stream_timeout() < encoded_buffer->receive_timestamp) {
			//Old stream hasn't been terminated successfully.
			//TODO: Cleanup packets which are too old?
            this->packet_queue.force_replay = encoded_buffer;
		} else if(encoded_buffer->buffer.empty()) {
			//Flush replay and stop
            this->packet_queue.force_replay = encoded_buffer;
		}

		if(packet_id_less(encoded_buffer->packet_id, this->packet_queue.last_packet_id, MAX_LOST_PACKETS) || encoded_buffer->packet_id == this->packet_queue.last_packet_id) {
			log_debug(category::voice_connection,
					tr("Received audio packet which is older than the current index (packet: {}, current: {})"), encoded_buffer->packet_id, this->packet_queue.last_packet_id);
			return;
		}

		/* insert the new buffer */
		{
			EncodedBuffer* prv_head{nullptr};
			auto head{this->packet_queue.pending_buffers};
			while(head && packet_id_less(head->packet_id, encoded_buffer->packet_id, MAX_LOST_PACKETS)) {
				prv_head = head;
				head = head->next;
			}

			encoded_buffer->next = head;
			if(prv_head) {
				prv_head->next = encoded_buffer;
			} else {
                this->packet_queue.pending_buffers = encoded_buffer;
			}
		}
        this->packet_queue.last_packet_timestamp = encoded_buffer->receive_timestamp;
        this->packet_queue.process_pending = true;
	}

	audio::decode_event_loop->schedule(dynamic_pointer_cast<event::EventEntry>(this->ref()));
}

void VoiceClient::cancel_replay() {
	log_trace(category::voice_connection, tr("Cancel replay for client {}"), this->client_id_);

	auto output = this->output_source;
	if(output) {
        output->clear();
	}

	audio::decode_event_loop->cancel(static_pointer_cast<event::EventEntry>(this->ref()));
	{
		auto execute_lock = this->execute_lock(true);
		this->drop_enqueued_buffers();
	}

	this->set_state(state::stopped);
}

void VoiceClient::drop_enqueued_buffers() {
    auto head = std::exchange(this->packet_queue.pending_buffers, nullptr);
    while(head) {
        delete std::exchange(head, head->next);
    }

    this->packet_queue.pending_buffers = nullptr;
    this->packet_queue.force_replay = nullptr;
}

void VoiceClient::event_execute(const std::chrono::system_clock::time_point &scheduled) {
    if(!this->output_source) {
        /* Audio hasn't been initialized yet. This also means there is no audio to be processed. */
		this->drop_enqueued_buffers();
        return;
    }

	static auto max_time = chrono::milliseconds(10);
	string error;

    std::unique_lock lock{this->packet_queue.pending_lock};
    while(this->packet_queue.process_pending) {
        assert(lock.owns_lock());
        EncodedBuffer* replay_head{nullptr};
        uint16_t local_last_pid{this->packet_queue.last_packet_id};

        /* nothing to play */
        if(!this->packet_queue.pending_buffers) {
            this->packet_queue.process_pending = false;
            break;
        }

        if(this->packet_queue.force_replay) {
            replay_head = this->packet_queue.pending_buffers;
            this->packet_queue.pending_buffers = this->packet_queue.force_replay->next;

            this->packet_queue.force_replay->next = nullptr;
            this->packet_queue.force_replay = nullptr;
        } else {
            EncodedBuffer* prv_head{nullptr};
            EncodedBuffer* head{nullptr};

            //Trying to replay the sequence
            head = this->packet_queue.pending_buffers;
            while(head && head->packet_id == this->packet_queue.last_packet_id + 1) {
                if(!replay_head) {
                    replay_head = this->packet_queue.pending_buffers;
                }

                this->packet_queue.last_packet_id++;
                prv_head = head;
                head = head->next;
            }
            this->packet_queue.pending_buffers = head;

            if(prv_head) {
                prv_head->next = nullptr; /* mark the tail */
            } else {
                assert(!replay_head); /* could not be set, else prv_head would be set */

                //No packet found here, test if we've more than n packets in a row somewhere
#define SKIP_SEQ_LENGTH (3)
                EncodedBuffer* skip_ptr[SKIP_SEQ_LENGTH + 1];
                memset(skip_ptr, 0, sizeof(skip_ptr));
                skip_ptr[0] = this->packet_queue.pending_buffers;

                while(skip_ptr[0]->next) {
                    for(size_t i = 0; i < SKIP_SEQ_LENGTH; i++) {
                        if(!skip_ptr[i]->next || skip_ptr[i]->packet_id + 1 != skip_ptr[i]->next->packet_id) {
                            break;
                        }

                        skip_ptr[i + 1] = skip_ptr[i]->next;
                    }

                    if(skip_ptr[SKIP_SEQ_LENGTH]) {
                        break;
                    }

                    skip_ptr[0] = skip_ptr[0]->next;
                }

                if(skip_ptr[SKIP_SEQ_LENGTH]) {
                    /* we've three packets in a row */
                    replay_head = this->packet_queue.pending_buffers;
                    this->packet_queue.pending_buffers = skip_ptr[SKIP_SEQ_LENGTH];
                    skip_ptr[SKIP_SEQ_LENGTH - 1]->next = nullptr;
                    log_trace(category::voice_connection, tr("Skipping from {} to {} because of {} packets in a row"), this->packet_queue.last_packet_id, replay_head->packet_id, SKIP_SEQ_LENGTH);

                    /*
                     * Do not set process_pending to false, because we're not done
                     * We're just replaying all loose packets which are not within a sequence until we reach a sequence
                     * In the next loop the sequence will be played
                     */
                } else {
                    head = this->packet_queue.pending_buffers;
                    while(head) {
                        if(packet_id_diff(this->packet_queue.last_packet_id, head->packet_id) >= 5) {
                            break;
                        }

                        head = head->next;
                    }

                    if(head) {
                        replay_head = this->packet_queue.pending_buffers;
                        this->packet_queue.pending_buffers = head->next;
                        head->next = nullptr;
                        log_trace(category::voice_connection, tr("Skipping from {} to {} because of over 6 packets between"),
                                this->packet_queue.last_packet_id, replay_head->packet_id);
                        /* do not negate process_pending here. Same reason as with the 3 sequence */
                    } else {
                        /* no packets we're willing to replay */
                        this->packet_queue.process_pending = false;
                    }
                }
            }
        }

        if(!replay_head) {
            this->packet_queue.process_pending = false;
            break;
        }

        {
            auto head = replay_head;
            while(head->next) {
                head = head->next;
            }

            this->packet_queue.last_packet_id = head->packet_id;
            const auto ordered = !this->packet_queue.pending_buffers || packet_id_less(this->packet_queue.last_packet_id, this->packet_queue.pending_buffers->packet_id, 10);
            if(!ordered) {
                log_critical(category::voice_connection, tr("Unordered packet ids. [!this->packet_queue.pending_buffers: {}; a: {}; b: {}]"),
                             !this->packet_queue.pending_buffers,
                             this->packet_queue.last_packet_id, this->packet_queue.pending_buffers->packet_id
                );
                //assert(!this->packet_queue.pending_buffers || packet_id_less(this->packet_queue.last_packet_id, this->packet_queue.pending_buffers->packet_id, 10));
            }
        }
        lock.unlock();

        while(replay_head) {
            if(replay_head->buffer.empty()) {
                switch(this->state_) {
                    case state::playing:
                    case state::buffering:
                        this->set_state(state::stopping);
                        log_debug(category::voice_connection, tr("Client {} send a stop signal. Flushing stream and stopping"), this->client_id_);
                        break;

                    case state::stopping:
                    case state::stopped:
                        break;

                    default:
                        assert(false);
                        break;
                }
            } else {
                bool reset_decoder{false};
                auto lost_packets = packet_id_diff(local_last_pid, replay_head->packet_id) - 1;
                if(lost_packets > 10) {
                    log_debug(category::voice_connection, tr("Client {} seems to be missing {} packets in stream ({} to {}). Resetting decoder."), this->client_id_, lost_packets, local_last_pid, replay_head->packet_id);
                    reset_decoder = true;
                } else if(lost_packets > 0) {
                    log_debug(category::voice_connection, tr("Client {} seems to be missing {} packets in stream. FEC decoding it."), this->client_id_, lost_packets);
                    /*
                    if(this->packet_queue.converter->decode_lost(error, lost_packets))
                        log_warn(category::audio, tr("Failed to decode lost packets for client {}: {}"), this->_client_id, error);
                    */

                    /* TODO: Notify the decoder about the lost decode packet? */
                    /* Reconstructing and replaying the lost packet by fec decoding the next known packet */
                    this->playback_audio_packet(replay_head->codec, replay_head->buffer.data_ptr(), replay_head->buffer.length(), true);
                }

                bool is_new_audio_stream;
                switch(this->state_) {
                    case state::stopped:
                    case state::stopping:
                        is_new_audio_stream = true;
                        break;

                    case state::buffering:
                    case state::playing:
                        is_new_audio_stream = false;
                        break;

                    default:
                        assert(false);
                        is_new_audio_stream = false;
                        break;
                }

                if(reset_decoder || is_new_audio_stream) {
                    this->reset_decoder(false);
                }

                this->playback_audio_packet(replay_head->codec, replay_head->buffer.data_ptr(), replay_head->buffer.length(), false);
            }

            local_last_pid = replay_head->packet_id;

            delete std::exchange(replay_head, replay_head->next);
        }

        /*
         * Needs to be locked when entering the loop.
         * We'll check for more packets.
         */
        lock.lock();
    };
}

void VoiceClient::reset_decoder(bool deallocate) {
	this->decoder.decoder_initialized = false;
	if(deallocate) {
		this->decoder.decoder = nullptr;
		this->decoder.resampler = nullptr;
		this->decoder.current_codec = AudioCodec::Unknown;
	} else if(this->decoder.decoder) {
		this->decoder.decoder->reset_sequence();
	}
}

constexpr static auto kTempBufferSampleSize{1024 * 8};
void VoiceClient::playback_audio_packet(uint8_t protocol_codec_id, const void *payload, size_t payload_size, bool fec_decode) {
    auto payload_codec = audio::codec::audio_codec_from_protocol_id(protocol_codec_id);
    if(!payload_codec.has_value()) {
        log_trace(category::audio, tr("Received packet with unknown audio codec id ({})."), (size_t) protocol_codec_id);
        return;
    }

	if(this->decoder.current_codec != *payload_codec) {
		if(fec_decode) {
			log_debug(category::audio, tr("Trying to fec decode audio packet but decoder hasn't been initialized with that codec. Dropping attempt."));
			return;
		}

		this->decoder.decoder_initialized = false;
		this->decoder.decoder = nullptr;
        this->decoder.current_codec = *payload_codec;

        if(!audio::codec::audio_decode_supported(this->decoder.current_codec)) {
            log_warn(category::voice_connection, tr("Client {} using and unsupported audio codec ({}). Dropping all its audio data."),
                     this->client_id_, audio::codec::audio_codec_name(this->decoder.current_codec));
            return;
        }

        this->decoder.decoder = audio::codec::create_audio_decoder(this->decoder.current_codec);
        if(!this->decoder.decoder) {
            log_error(category::voice_connection, tr("Failed to create decoder for audio codec {}."), audio::codec::audio_codec_name(this->decoder.current_codec));
            return;
        }

		std::string error{};
        if(!this->decoder.decoder->initialize(error)) {
            log_error(category::voice_connection, tr("Failed to initialize {} decoder: {}"), audio::codec::audio_codec_name(this->decoder.current_codec), error);
            this->decoder.decoder = nullptr;
            return;
        }
	}

	if(!this->decoder.decoder) {
		/* Decoder failed to initialize. Dropping all packets. */
		return;
	}

	float temp_buffer[kTempBufferSampleSize];
	const auto decoder_channel_count = this->decoder.decoder->channel_count();

	if(!this->decoder.decoder_initialized) {
		if(fec_decode) {
			log_debug(category::audio, tr("Trying to fec decode audio packet but decoder hasn't been initialized with that codec. Dropping attempt."));
			return;
		}

		/*
		 * We're fec decoding so we need to pass the amount of samples we want to decode.
		 * Usually a network packet contains 20ms of audio data.
		 */
		auto sample_count{(size_t) (this->decoder.decoder->sample_rate() * 0.02)};

        DecodePayloadInfo decode_info{};
        decode_info.fec_decode = true;
        decode_info.byte_length = payload_size;

        std::string error{};
		if(!this->decoder.decoder->decode(error, temp_buffer, sample_count, decode_info, payload)) {
		    log_warn(category::audio, tr("Failed to initialize decoder with fec data: {}"), error);
		}

		this->decoder.decoder_initialized = true;
	}


    size_t current_sample_rate;
	size_t current_sample_count;
	size_t current_channel_count;
    {
        std::string error{};
        auto sample_count{kTempBufferSampleSize / decoder_channel_count};
        if(fec_decode) {
            /*
             * See notes for the decoder initialisation.
             */
            sample_count = (size_t) (this->decoder.decoder->sample_rate() * 0.02);
        }

        DecodePayloadInfo decode_info{};
        decode_info.fec_decode = fec_decode;
        decode_info.byte_length = payload_size;
        if(!this->decoder.decoder->decode(error, temp_buffer, sample_count, decode_info, payload)) {
            log_warn(category::audio, tr("Failed to decode audio packet (fec: {}): {}"), fec_decode, error);
        }

        current_sample_count = sample_count;
        current_channel_count = this->decoder.decoder->channel_count();
        current_sample_rate = this->decoder.decoder->sample_rate();
    }

    auto audio_output = this->output_source;
    if(!audio_output) {
        /*
         * We have no target to replay the audio.
         * We're only doing it here and not earlier to provide the decoder with the required info of the audio sequence
         * so when we actually have audio we're not lacking behind and having some artefacts.
         */
        return;
    }

    if(this->volume_ == 0) {
        /* Client has been muted */
        return;
    }

    const auto output_channel_count = audio_output->channel_count();
    if(current_channel_count != output_channel_count) {
        if(kTempBufferSampleSize < output_channel_count * current_sample_count) {
            log_error(category::voice_connection, tr("Temporary buffer can't hold {} samples ({} channels) of audio data. Audio frame too big. Dropping it."), current_sample_count, output_channel_count);
            return;
        }

        if(!audio::merge::merge_channels_interleaved(temp_buffer, output_channel_count, temp_buffer, current_channel_count, current_sample_count)) {
            log_warn(category::voice_connection, tr("Failed to merge channels to output stream channel count!"));
            return;
        }

        current_channel_count = output_channel_count;
    }

    const auto output_sample_rate = audio_output->sample_rate();
    if(current_sample_rate != output_sample_rate) {
        if(!this->decoder.resampler || this->decoder.resampler->output_rate() != output_sample_rate || this->decoder.resampler->input_rate() != current_sample_rate) {
            this->decoder.resampler = std::make_unique<audio::AudioResampler>(current_sample_rate, output_sample_rate, output_channel_count);
        }

        auto expected_output_samples = this->decoder.resampler->estimated_output_size(current_sample_count);
        if(expected_output_samples * output_channel_count > kTempBufferSampleSize) {
            log_error(category::voice_connection, tr("Temporary buffer can't hold the full resampled frame. Dropping it."), current_sample_count, output_channel_count);
            return;
        }

        size_t output_samples{expected_output_samples};
        if(!this->decoder.resampler->process(temp_buffer, temp_buffer, current_sample_count, output_samples)) {
            log_error(category::voice_connection, tr("Failed to resample audio codec sample rate to our audio output sample rate. Dropping audio frame."));
            return;
        }

        current_sample_rate = output_sample_rate;
        current_sample_count = output_samples;
    }

    audio::apply_gain(temp_buffer, current_channel_count, current_sample_count, this->volume_);

    assert(audio_output->sample_rate() == current_sample_rate);
    assert(audio_output->channel_count() == current_channel_count);
    audio_output->enqueue_samples(temp_buffer, current_sample_count);
    this->set_state(state::playing);
}

/*
 * This method will be called within the audio event loop.
 */
bool VoiceClient::handle_output_underflow(size_t sample_count) {
	switch (this->state_) {
		case state::stopping:
			/*
			 * No more data to play out.
			 * We've successfully replayed our queue and are now in stopped state.
			 */
			this->set_state(state::stopped);
			break;

		case state::stopped:
			/*
			 * We don't really care.
			 * We have no audio to play back.
			 */
			break;

		case state::playing:
			/*
			 * We're missing audio data.
			 * Lets go back to buffering.
			 */
			this->set_state(state::buffering);
			break;

		case state::buffering:
			/*
			 * Seems like we don't have any data for a bit longer already.
			 * Lets check if we timeout this stream.
			 */
			if(this->packet_queue.last_packet_timestamp + std::chrono::seconds{1} < std::chrono::system_clock::now()) {
				this->set_state(state::stopped);
				log_warn(category::audio, tr("Clients {} audio stream timed out. We haven't received any audio packed within the last second. Stopping replay."), this->client_id_, sample_count);
			} else {
				/*
				 * Lets wait until we have the next audio packet.
				 */
			}
			break;

	}

	/*
	 * We haven't filled up the buffer.
	 */
	return false;
}