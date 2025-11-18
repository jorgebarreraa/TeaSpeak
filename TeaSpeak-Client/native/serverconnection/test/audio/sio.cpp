#include <soundio/soundio.h>

#include <cmath>
#include <iostream>
#include <string>
#include <chrono>
#include <mutex>
#include "../../src/logger.h"
#include "../../src/audio/driver/SoundIO.h"

using namespace std;
using namespace tc::audio;

static const float PI = 3.1415926535f;
static float seconds_offset = 0.0f;

auto next_write = chrono::system_clock::now();
static void write_callback(struct SoundIoOutStream *outstream,
                           int frame_count_min, int frame_count_max)
{
	const struct SoundIoChannelLayout *layout = &outstream->layout;
	float float_sample_rate = outstream->sample_rate;
	float seconds_per_frame = 1.0f / float_sample_rate;
	struct SoundIoChannelArea *areas;
	int frames_left = 960 ;
	int err;

	if(next_write > chrono::system_clock::now()) {
		return;
	}
	cout << frame_count_min << "/" << frame_count_max << endl;
	next_write = chrono::system_clock::now() + chrono::milliseconds{20};

	while (frames_left > 0) {
		int frame_count = frames_left;

		if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
			fprintf(stderr, "%s\n", soundio_strerror(err));
			exit(1);
		}

		if (!frame_count)
			break;

		float pitch = 440.0f;
		float radians_per_second = pitch * 2.0f * PI;
		for (int frame = 0; frame < frame_count; frame += 1) {
			float sample = sinf((seconds_offset + frame * seconds_per_frame) * radians_per_second);
			for (int channel = 0; channel < layout->channel_count; channel += 1) {
				float *ptr = (float*)(areas[channel].ptr + areas[channel].step * frame);
				*ptr = sample;
			}
		}
		seconds_offset = fmodf(seconds_offset +
		                       seconds_per_frame * frame_count, 1.0f);

		if ((err = soundio_outstream_end_write(outstream))) {
			fprintf(stderr, "%s\n", soundio_strerror(err));
			exit(1);
		}

		frames_left -= frame_count;
	}

	cout << "FLeft: " << frames_left << endl;
}

int main(int argc, char **argv) {
	logger::initialize_raw();
	SoundIOBackendHandler::initialize_all();
    SoundIOBackendHandler::connect_all();

    const auto print_device = [](const std::shared_ptr<SoundIODevice>& device) {
        std::cout << "  - " << device->id() << " (" << device->name() << ")";
        if(device->is_output_default() || device->is_input_default())
            std::cout << " [Default]";
        std::cout << "\n";
    };
	for(auto& backend : tc::audio::SoundIOBackendHandler::all_backends()) {
	    std::cout << "Backend " << backend->name() << ":\n";
        std::cout << " Input devices: (" << backend->input_devices().size() << "): \n";
        for(auto& device : backend->input_devices()) print_device(device);
        std::cout << " Output devices: (" << backend->input_devices().size() << "): \n";
        for(auto& device : backend->input_devices()) print_device(device);
	}

    SoundIOBackendHandler::shutdown_all();
	return 0;
    int err;
	struct SoundIo *soundio = soundio_create();
	if (!soundio) {
		fprintf(stderr, "out of memory\n");
		return 1;
	}

	if ((err = soundio_connect(soundio))) {
		fprintf(stderr, "error connecting: %s", soundio_strerror(err));
		return 1;
	}


	soundio_flush_events(soundio);
	cout << "BCound: " << soundio_backend_count(soundio) << endl;
	for(int i = 0; i < soundio_backend_count(soundio); i++)
		cout << i << " => " << soundio_backend_name(soundio_get_backend(soundio, i)) << endl;

	for(int i = 0; i < soundio_input_device_count(soundio); i++) {
		auto dev = soundio_get_input_device(soundio, i);
		cout << dev->name << " - " << dev->id << endl;
        soundio_device_unref(dev);
	}


	int default_out_device_index = soundio_default_output_device_index(soundio);
	if (default_out_device_index < 0) {
		fprintf(stderr, "no output device found");
		return 1;
	}

	struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
	if (!device) {
		fprintf(stderr, "out of memory");
		return 1;
	}
	fprintf(stderr, "Output device: %s\n", device->name);

	for(int i = 0; i < 1; i++) {
		struct SoundIoOutStream *outstream = soundio_outstream_create(device);
		outstream->format = SoundIoFormatFloat32LE;
		outstream->write_callback = write_callback;
		outstream->software_latency = 0.02;
		outstream->underflow_callback = [](auto ptr) {
			cout << "Underflow" << endl;
		};
		outstream->error_callback = [](auto ptr, auto code) {
			cout << "Error:" << code << endl;
		};
		if ((err = soundio_outstream_open(outstream))) {
			fprintf(stderr, "unable to open device: %s", soundio_strerror(err));
			return 1;
		}

		if (outstream->layout_error)
			fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));

		if ((err = soundio_outstream_start(outstream))) {
			fprintf(stderr, "unable to start device: %s", soundio_strerror(err));
			return 1;
		}
	}

	for (;;)
		soundio_wait_events(soundio);

	//soundio_outstream_destroy(outstream);
	soundio_device_unref(device);
	soundio_destroy(soundio);
	return 0;
}