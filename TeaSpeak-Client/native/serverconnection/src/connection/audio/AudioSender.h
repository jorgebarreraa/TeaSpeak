#pragma once

#include <mutex>
#include <memory>
#include <optional>
#include "./VoiceClient.h"

namespace tc {
	namespace audio {
		namespace codec {
			class Converter;
			class AudioEncoder;
		}

        class AudioReframer;
		class AudioResampler;
		class AudioOutputSource;
	}

	namespace connection {
		class VoiceConnection;
		class VoiceSender : public event::EventEntry {
				friend class VoiceConnection;
			public:
		        using AudioCodec = audio::codec::AudioCodec;

				explicit VoiceSender(VoiceConnection*);
				virtual ~VoiceSender();

                void finalize();

                [[nodiscard]] inline auto target_codec() const { return this->target_codec_; }
                inline void set_codec(const AudioCodec& target) { this->target_codec_ = target; }

				void send_data(const float* /* buffer */, size_t /* samples */, size_t /* sample rate */, size_t /* channels */);
				void send_stop();

				void set_voice_send_enabled(bool /* flag */);
			private:
                struct AudioFrame {
                    AudioFrame* next{nullptr};

                    float* buffer{nullptr};
                    size_t sample_count{0};
                    size_t sample_rate{0};
                    size_t channels{0};

                    std::chrono::system_clock::time_point timestamp{};

                    ~AudioFrame() = default;
                    AudioFrame() = default;
                };

				std::weak_ptr<VoiceSender> _ref;
				VoiceConnection* handle;

				std::mutex raw_audio_buffer_mutex{};
                AudioFrame* raw_audio_buffers_head{nullptr};
                AudioFrame** raw_audio_buffers_tail{&this->raw_audio_buffers_head};

				bool voice_send_enabled{false};

				/* Codec specific values */
                AudioCodec target_codec_{AudioCodec::Unknown};
                AudioCodec current_codec{AudioCodec::Unknown};

                std::unique_ptr<audio::codec::AudioEncoder> codec_encoder{};
                std::unique_ptr<audio::AudioResampler> codec_resampler{};
                std::unique_ptr<audio::AudioReframer> codec_reframer{};

                size_t audio_sequence_no{0};

                /* Call these methods only when the execute lock has been accquired */
				void encode_raw_frame(const AudioFrame*);
				void handle_network_frame(const float* /* buffer */, size_t /* sample count */, bool /* flush */);
				void flush_current_codec();

				void event_execute(const std::chrono::system_clock::time_point &point) override;
		};
	}
}