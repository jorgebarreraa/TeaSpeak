//
// Created by wolverindev on 19.06.19.
//

#include "AudioEventLoop.h"
using namespace tc;

event::EventExecutor* audio::decode_event_loop = nullptr;
event::EventExecutor* audio::encode_event_loop = nullptr;

void audio::init_event_loops() {
	audio::shutdown_event_loops(); /* just to ensure */

	audio::decode_event_loop = new event::EventExecutor("a en/decode ");
	audio::encode_event_loop = audio::decode_event_loop;

	audio::decode_event_loop->initialize(2);
}

void audio::shutdown_event_loops() {
	if(audio::decode_event_loop) {
		delete audio::decode_event_loop;
		audio::decode_event_loop = nullptr;
	}
	audio::encode_event_loop = nullptr;
}