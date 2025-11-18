#include <thread>
#include <vector>
#include <condition_variable>
#include <cassert>
#include "EventLoop.h"

using namespace std;
using namespace tc::event;

EventExecutor::EventExecutor(const std::string& thread_prefix) : thread_prefix(thread_prefix) {}
EventExecutor::~EventExecutor() {
	unique_lock lock(this->lock);
	this->_shutdown(lock);
	this->_reset_events(lock);
}

bool EventExecutor::initialize(int threads) {
	unique_lock lock(this->lock);
	this->_shutdown(lock);
	if(!lock.owns_lock())
		lock.lock();

	this->should_shutdown = false;
	while(threads-- > 0) {
		this->threads.emplace_back(&EventExecutor::_executor, this);

		#ifndef WIN32
		{
			auto handle = this->threads.back().native_handle();
			auto name = this->thread_prefix + to_string(this->threads.size());
			pthread_setname_np(handle, name.c_str());
		}
		#endif
	}
	return true;
}

void EventExecutor::shutdown() {
	unique_lock lock(this->lock);
	this->_shutdown(lock);
}

bool EventExecutor::schedule(const std::shared_ptr<tc::event::EventEntry> &entry) {
	unique_lock lock(this->lock);
	if(!entry || entry->_event_ptr)
		return true; /* already scheduled */

	auto linked_entry = new LinkedEntry{};
	linked_entry->entry = entry;
	linked_entry->next = nullptr;
	linked_entry->previous = this->tail;
	linked_entry->scheduled = std::chrono::system_clock::now();

	if(this->tail) {
		this->tail->next = linked_entry;
		this->tail = linked_entry;
	} else {
		this->head = linked_entry;
		this->tail = linked_entry;
	}
	this->condition.notify_one();

	entry->_event_ptr = linked_entry;
	return true;
}

bool EventExecutor::cancel(const std::shared_ptr<tc::event::EventEntry> &entry) {
	unique_lock lock(this->lock);
	if(!entry || !entry->_event_ptr)
		return false;

	auto linked_entry = (LinkedEntry*) entry->_event_ptr;
	this->head = linked_entry->next;
	if(this->head) {
		assert(linked_entry == this->head->previous);
		this->head->previous = nullptr;
	} else {
		assert(linked_entry == this->tail);
		this->tail = nullptr;
	}

	delete linked_entry;
	entry->_event_ptr = nullptr;
	return true;
}

void EventExecutor::_shutdown(std::unique_lock<std::mutex> &lock) {
	if(!lock.owns_lock())
		lock.lock();

	this->should_shutdown = true;
	this->condition.notify_all();
	lock.unlock();
	for(auto& thread : this->threads)
		thread.join(); /* TODO: Timeout? */
	lock.lock();
	this->should_shutdown = false;
}

void EventExecutor::_reset_events(std::unique_lock<std::mutex> &lock) {
	if(!lock.owns_lock())
		lock.lock();
	auto entry = this->head;
	while(entry) {
		auto next = entry->next;
		delete entry;
		entry = next;
	}
	this->head = nullptr;
	this->tail = nullptr;
}

void EventExecutor::_executor(tc::event::EventExecutor *loop) {
	while(true) {
		unique_lock lock(loop->lock);
		loop->condition.wait(lock, [&] { return loop->should_shutdown || loop->head != nullptr; });
		if(loop->should_shutdown)
			break;

		if(!loop->head)
			continue;

		auto linked_entry = loop->head;
		loop->head = linked_entry->next;
		if(loop->head) {
			assert(linked_entry == loop->head->previous);
			loop->head->previous = nullptr;
		} else {
			assert(linked_entry == loop->tail);
			loop->tail = nullptr;
		}

		auto event_handler = linked_entry->entry.lock();
		assert(event_handler->_event_ptr == linked_entry);
		event_handler->_event_ptr = nullptr;
		lock.unlock();

		if(event_handler) {
			if(event_handler->single_thread_executed()) {
				auto execute_lock = event_handler->execute_lock(false);
				if(!execute_lock) {
					event_handler->event_execute_dropped(linked_entry->scheduled);
				} else {
					event_handler->event_execute(linked_entry->scheduled);
				}
			} else {
				event_handler->event_execute(linked_entry->scheduled);
			}
		}
		delete linked_entry;
	}
}