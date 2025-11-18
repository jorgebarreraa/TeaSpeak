#include <cstring>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

#include "../../src/audio/AudioOutput.h"
#include "../../src/audio/AudioInput.h"
#include "../../src/audio/AudioEventLoop.h"
#include "../../src/audio/filter/FilterVad.h"
#include "../../src/audio/filter/FilterThreshold.h"
#include "../../src/logger.h"

using namespace std;
using namespace tc;

tc::audio::AudioOutput* global_audio_output{nullptr};
int main() {
    std::string error{};

    logger::initialize_raw();
    tc::audio::init_event_loops();
    tc::audio::initialize();
    tc::audio::await_initialized();

    std::shared_ptr<tc::audio::AudioDevice> default_playback{nullptr}, default_record{nullptr};
    for(auto& device : tc::audio::devices()) {
        if(device->is_output_default()) {
            default_playback = device;
        }

        if(device->is_input_default()) {
            default_record = device;
        }
    }
    assert(default_record);
    assert(default_playback);

    for(auto& dev : tc::audio::devices()) {
        if(!dev->is_input_supported()) {
            continue;
        }

        auto playback_manager = std::make_unique<audio::AudioOutput>(2, 48000);
        global_audio_output = &*playback_manager;

        playback_manager->set_device(default_playback);
        if(!playback_manager->playback(error)) {
            cerr << "failed to start playback: " << error << endl;
            return 1;
        }

        auto input = std::make_unique<audio::AudioInput>(2, 48000);
        input->set_device(default_record);

        if(!input->record(error)) {
            cerr << "failed to start record for " << dev->id() << " (" << dev->name() << "): " << error << endl;
            continue;
        }

        {
            auto target_stream = playback_manager->create_source();

            auto consumer = input->create_consumer(960);
            auto vad_handler = make_shared<audio::filter::VadFilter>(2, 48000, 960);
            if(!vad_handler->initialize(error, 3, 4)) {
                cerr << "failed to initialize vad handler (" << error << ")";
                return 1;
            }

            auto threshold_filter = make_shared<audio::filter::ThresholdFilter>(2, 48000, 960);
            if(!threshold_filter->initialize(error, .5, 5)) {
                cerr << "failed to initialize threashold handler (" << error << ")";
                return 1;
            }

            consumer->on_read = [target_stream, vad_handler, threshold_filter](const void* buffer, size_t samples) { //Do not capture consumer!
                target_stream->enqueue_samples(buffer, samples);

                /*
                const auto analize_value = threshold_filter->analyze(buffer, 0);
                if(vad_handler->process(buffer)) {
                    cout << "Read " << samples << " (" << analize_value << ")" << endl;
                    target_stream->enqueue_samples(buffer, samples);
                } else {
                    cout << "Drop " << samples << " (" << analize_value << ")" << endl;
                }
                 */
            };

            input.release();
            cout << "Read started" << endl;
        }

        playback_manager.release(); //FIXME: Memory leak!
        break;
    }

	this_thread::sleep_for(chrono::seconds(360));

	/*
	while(true) {
		this_thread::sleep_for(chrono::seconds(1000));
	}
	*/
	return 1;
}