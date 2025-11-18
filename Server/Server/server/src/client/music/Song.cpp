#include <chrono>
#include <thread>

#include "Song.h"

using namespace ts;
using namespace ts::server;
using namespace ts::music;
using namespace std;
using namespace std::chrono;

PlayableSong::~PlayableSong() = default;

shared_ptr<PlayableSong::song_future_t> PlayableSong::get_loader(const std::shared_ptr<server::VirtualServer>& server, bool spawn_new) {
    if(!spawn_new || this->_loader_future) return this->_loader_future;

    this->_loader_future = make_shared<PlayableSong::song_future_t>();

    auto ref = this->ref();
    auto future = this->_loader_future;
    auto function = this->_loader_function;

    if(function && ref) {
        weak_ptr weak_server = server;
        /* async loading */
        std::thread([weak_server, ref, future, function]{
            auto server = weak_server.lock();
            if(!server) {
                future->executionFailed("broken server reference");
                return;
            }
            string error;
            try {
                auto result = (*function)(server, ref, error);
                if(result) {
                    ref->data_loaded = result;
                    future->executionSucceed(std::move(result));
                } else {
                    if(error.empty())
                        error = "empty result";

                    future->executionFailed(error);
                }
            } catch(std::exception& ex) {
                future->executionFailed("Got an error while loading song. (" + string(ex.what()) + ")");
            }
            /*
            //Never throw non std::exception exceptions!
            catch(...) {

            }
            */
        }).detach();
    } else {
        if(ref)
            this->_loader_future->executionFailed("missing loader function");
        else
            this->_loader_future->executionFailed("self lock expired");
    }

    return this->_loader_future;
}