#include <iostream>

#include <mutex>
#include <condition_variable>
#include <cassert>
#include <thread>
#include <event.h>
#include <unbound.h>
#include <unbound-event.h>

#define ITERATIONS (2000)

int main() {
	/* Setting up all libevent stuff */
	std::mutex ev_lock{};
	std::condition_variable ev_condition{};
	auto ev_base = event_base_new();

	std::thread ev_loop = std::thread([&]{
		/* Wait until we should start the event loop */
		{
			std::unique_lock lock{ev_lock};
			ev_condition.wait(lock);
		}

		/* executing the event loop until no events are registered */
		event_base_loop(ev_base, 0);
	});


	/* Setting up libunbound */
	auto ctx = ub_ctx_create_event(ev_base);
	assert(ctx);

	auto errc = ub_ctx_hosts(ctx, nullptr);
	assert(errc == 0);

	errc = ub_ctx_resolvconf(ctx, nullptr);
	assert(errc == 0);

	int callback_call_count{0};
	for(int i = 0; i < ITERATIONS; i++){
		auto host = std::to_string(i) + "ts.teaspeak.de";
		errc = ub_resolve_event(ctx, host.c_str(), 33, 1, (void*) &callback_call_count, [](void* _ccc, int, void*, int, int, char*) {
			*static_cast<int*>(_ccc) += 1;
		}, nullptr);
		assert(errc == 0);

		if(i == 0) {
			//CASE 1: CRASH
			//Start the event loop as soon the first resolve has been scheduled
			ev_condition.notify_one();
		}
	}

	//CASE 2: NO CRASH
	//Start the event loop after all resolves have been scheduled
	//ev_condition.notify_one();

	/* Hang up until no more events */
	ev_loop.join();
	assert(ITERATIONS == callback_call_count);

	ub_ctx_delete(ctx);
	event_base_free(ev_base);
}