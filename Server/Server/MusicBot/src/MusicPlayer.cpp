#include <dlfcn.h>
#include <algorithm>
#include <log/LogUtils.h>
#include <experimental/filesystem>
#include "teaspeak/MusicPlayer.h"

using namespace std;
using namespace music;
using namespace music::manager;

namespace fs = std::experimental::filesystem;

void log::log(const Level& lvl, const std::string& msg) {
    logger::logger(0)->log((spdlog::level::level_enum) lvl, "[Music] " + msg);
}

void AbstractMusicPlayer::registerEventHandler(const std::string& key, const std::function<void(MusicEvent)>& function) {
    std::lock_guard lock(this->eventLock);
    this->eventHandlers.emplace_back(key, function);
}

void AbstractMusicPlayer::unregisterEventHandler(const std::string& string) {
    std::lock_guard lock(this->eventLock);
    for(const auto& entry : this->eventHandlers){
        if(entry.first == string) {
            this->eventHandlers.erase(find_if(this->eventHandlers.begin(), this->eventHandlers.end(), [string](const std::pair<std::string, std::function<void(MusicEvent)>>& elm){ return elm.first == string; }));
            break;
        }
    }
}

void AbstractMusicPlayer::fireEvent(MusicEvent event) {
    decltype(this->eventHandlers) handlers{};
    {
        std::lock_guard lock(this->eventLock);
        handlers = this->eventHandlers; //Copy for remove while fire
    }
    for(const auto& entry : handlers)
        entry.second(event);
}

const char* music::stateNames[] = {"uninitialised", "playing", "paused", "stopped"};

static std::mutex staticLock;
static std::deque<std::shared_ptr<PlayerProvider>> types;

std::deque<std::shared_ptr<PlayerProvider>> manager::registeredTypes(){ return types; }
void registerType(const std::shared_ptr<PlayerProvider>& provider) {
    std::lock_guard l(staticLock);
    types.push_back(provider);
}

//empty for not set
std::shared_ptr<PlayerProvider> manager::resolveProvider(const std::string& provName, const std::string& str) {
    std::lock_guard l(staticLock);
    vector<std::shared_ptr<PlayerProvider>> provs;
    for(const auto& prov : types){
        auto p = prov.get();
        if(!str.empty() && prov->acceptString(str))
            provs.push_back(prov);
        else if(!provName.empty() && prov->providerName == provName)
            provs.push_back(prov);
    }
    sort(provs.begin(), provs.end(), [str](const std::shared_ptr<PlayerProvider>& a, const std::shared_ptr<PlayerProvider>& b){
        return a->weight(str) > b->weight(str);
    });
    return provs.empty() ? nullptr : provs.front();
}

typedef std::shared_ptr<music::manager::PlayerProvider>(*create_provider_fn)();
void manager::loadProviders(const std::string& path) {
    auto dir = fs::u8path(path);
    if(!fs::exists(dir)){
        try {
            fs::create_directories(dir);
        } catch (std::exception& e) {}
        return;
    }

    deque<fs::path> paths;
    error_code error_code{};
    for(const auto& entry : fs::directory_iterator(dir, error_code)){
        if(!entry.path().has_extension()) continue;
        if(entry.path().extension().string() == ".so")
            paths.push_back(entry.path());
    }
    if(error_code) {
        log::log(log::err, "Failed to scan the target directory (" + dir.string() + "): " + error_code.message());
        return;
    }
    std::sort(paths.begin(), paths.end(), [](const fs::path& a, const fs::path& b){ return a.filename().string() < b.filename().string(); });

    int index = 0;
    log::log(log::debug, "Provider load order:");
    for(const auto& entry : paths)
        log::log(log::debug, "[" + to_string(index++) + "] " + entry.string());

    for(const auto& entry : paths){
        void* provider = dlopen(entry.string().c_str(), RTLD_NOW);
        if(!provider){
            log::log(log::err, string() + "Could not load music provider " + entry.string() + ". Error: " + dlerror());
            continue;
        }
        auto create_provider = reinterpret_cast<create_provider_fn>(dlsym(provider, "create_provider"));
        if(!create_provider){
            log::log(log::err, string() + "Could not find entry point create_provider()@" + entry.string());
            dlclose(provider);
            continue;
        }
        auto mprovider = (*create_provider)();
        if(!mprovider){
            log::log(log::err, string() + "Could not create music provider for " + entry.string());
            dlclose(provider);
            continue;
        }
        log::log(log::info, string() + "Loaded successfully provider " +  mprovider->providerName);
        types.push_back(mprovider);
    }
}

void manager::register_provider(const std::shared_ptr<music::manager::PlayerProvider> &provider) {
    std::lock_guard l(staticLock);
    types.push_back(provider);
}

void manager::finalizeProviders() {
    std::lock_guard l(staticLock);
	types.clear();
}