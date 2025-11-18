//
// Created by wolverindev on 07.02.20.
//

#include "SoundIO.h"
#include <algorithm>
#include "../../logger.h"

using namespace tc::audio;

std::mutex SoundIOBackendHandler::backend_lock{};
std::vector<std::shared_ptr<SoundIOBackendHandler>> SoundIOBackendHandler::backends{};

std::shared_ptr<SoundIOBackendHandler> SoundIOBackendHandler::get_backend(SoundIoBackend backend_type) {
    std::lock_guard lock{backend_lock};
    for(auto& backend : SoundIOBackendHandler::backends)
        if(backend->backend == backend_type)
            return backend;

    return nullptr;
}

void SoundIOBackendHandler::initialize_all() {
    std::lock_guard lock{backend_lock};

    for(const auto& backend : {
            SoundIoBackendJack,
            SoundIoBackendPulseAudio,
            SoundIoBackendAlsa,
            SoundIoBackendCoreAudio,
            SoundIoBackendWasapi,
            SoundIoBackendDummy
    }) {
        if(!soundio_have_backend(backend)) {
            log_debug(category::audio, tr("Skipping audio backend {} because its not supported on this platform."), soundio_backend_name(backend));
            continue;
        }

        auto handler = std::make_shared<SoundIOBackendHandler>(backend);
        if(std::string error{}; !handler->initialize(error)) {
            log_error(category::audio, tr("Failed to initialize sound backed {}: {}"), soundio_backend_name(backend), error);
            continue;
        }

        backends.push_back(handler);
    }

    std::stable_sort(backends.begin(), backends.end(), [](const auto& a, const auto& b) { return a->priority() > b->priority(); });
}

void SoundIOBackendHandler::connect_all() {
    std::string error{};

    std::lock_guard lock{backend_lock};
    for(const auto& backend : backends)
        if(!backend->connect(error))
            log_error(category::audio, tr("Failed to connect to audio backend {}: {}"), backend->name(), error);

}

void SoundIOBackendHandler::shutdown_all() {
    std::lock_guard lock{backend_lock};
    for(auto& entry : backends)
        entry->shutdown();
    backends.clear();
}

SoundIOBackendHandler::SoundIOBackendHandler(SoundIoBackend backed) : backend{backed} {}
SoundIOBackendHandler::~SoundIOBackendHandler() {
    this->shutdown();
}

bool SoundIOBackendHandler::initialize(std::string &error) {
    assert(!this->soundio_handle);

    this->soundio_handle = soundio_create();
    if(!this->soundio_handle) {
        error = "out of memory";
        return false;
    }

    this->soundio_handle->userdata = this;
    this->soundio_handle->on_devices_change = [](auto handle){
        reinterpret_cast<SoundIOBackendHandler*>(handle->userdata)->handle_device_change();
    };
    this->soundio_handle->on_backend_disconnect = [](auto handle, auto err){
        reinterpret_cast<SoundIOBackendHandler*>(handle->userdata)->handle_backend_disconnect(err);
    };

    return true;
}

void SoundIOBackendHandler::shutdown() {
    if(!this->soundio_handle) return;

    soundio_destroy(this->soundio_handle);
    this->soundio_handle = nullptr;
}

bool SoundIOBackendHandler::connect(std::string &error, bool enforce) {
    if(!this->soundio_handle) {
        error = "invalid handle";
        return false;
    }

    if(this->_connected && !enforce) {
        error = "already connected";
        return false;
    }

    auto err = soundio_connect_backend(this->soundio_handle, this->backend);
    if(err) {
        error = soundio_strerror(err);
        return false;
    }

    this->soundio_handle->app_name = "TeaClient";
    this->_connected = true;

    {
        auto begin = std::chrono::system_clock::now();
        soundio_flush_events(this->soundio_handle);
        auto end = std::chrono::system_clock::now();
        log_debug(category::audio, tr("Flushed connect events within {}ms for backend {}"),
                std::chrono::ceil<std::chrono::milliseconds>(end - begin).count(),
                this->name());
    }
    return true;
}

void SoundIOBackendHandler::disconnect() {
    if(!this->soundio_handle || !this->_connected) return;

    soundio_disconnect(this->soundio_handle);
}

inline std::string sample_rates(struct ::SoundIoDevice *dev) {
    std::string result{};
    for(size_t index = 0; index < dev->sample_rate_count; index++)
        result += (index > 0 ? ", [" : "[") + std::to_string(dev->sample_rates[index].min) + ";" + std::to_string(dev->sample_rates[index].max) + "]";
    return dev->sample_rate_count ? result : "none";
}

void SoundIOBackendHandler::handle_device_change() {
    log_debug(category::audio, tr("Device list changed for backend {}. Reindexing devices."), this->name());

    std::lock_guard lock{this->device_lock};
    this->_default_output_device.reset();
    this->_default_input_device.reset();
    this->cached_input_devices.clear();
    this->cached_output_devices.clear();

    if(!this->_connected || !this->soundio_handle) return;

    size_t input_devices{0}, output_devices{0};
    auto default_input_device{soundio_default_input_device_index(this->soundio_handle)};
    for(int i = 0; i < soundio_input_device_count(this->soundio_handle); i++) {
        auto dev = soundio_get_input_device(this->soundio_handle, i);
        if(!dev) {
            log_warn(category::audio, tr("Failed to get input device at index {} for backend {}."), i, this->name());
            continue;
        }
        if(dev->probe_error) {
            log_trace(category::audio, tr("Skipping input device {} ({}) for backend {} because of probe error: {}"), dev->id, dev->name, this->name(), soundio_strerror(dev->probe_error));
            soundio_device_unref(dev);
            continue;
        }

        auto device = std::make_shared<SoundIODevice>(dev, this->name(), i == default_input_device, true);
        log_trace(category::audio, tr("Found input device {} ({}). Raw: {}. Rates: {}"), dev->id, dev->name, dev->is_raw, sample_rates(dev));
        this->cached_input_devices.push_back(device);
        if(i == default_input_device)
            this->_default_input_device = device;
        input_devices++;
    }

    auto default_output_device{soundio_default_output_device_index(this->soundio_handle)};
    for(int i = 0; i < soundio_output_device_count(this->soundio_handle); i++) {
        auto dev = soundio_get_output_device(this->soundio_handle, i);
        if(!dev) {
            log_warn(category::audio, tr("Failed to get output device at index {} for backend {}."), i, this->name());
            continue;
        }
        if(dev->probe_error) {
            log_trace(category::audio, tr("Skipping output device {} ({}) for backend {} because of probe error: {}"), dev->id, dev->name, this->name(), soundio_strerror(dev->probe_error));
            soundio_device_unref(dev);
            continue;
        }

        auto device = std::make_shared<SoundIODevice>(dev, this->name(), i == default_output_device, true);
        log_trace(category::audio, tr("Found output device {} ({}). Raw: {}. Rates: {}."), dev->id, dev->name, dev->is_raw, sample_rates(dev));
        this->cached_output_devices.push_back(device);
        if(i == default_output_device)
            this->_default_output_device = device;
        output_devices++;
    }

    log_info(category::audio, tr("Queried devices for backend {}, resulting in {} input and {} output devices."),
            this->name(),
            input_devices,
            output_devices
    );
}

void SoundIOBackendHandler::handle_backend_disconnect(int error) {
    log_info(category::audio, tr("Backend {} disconnected with error {}."), this->name(), soundio_strerror(error));
}

SoundIODevice::SoundIODevice(struct ::SoundIoDevice *dev, std::string driver, bool default_, bool owned) : device_handle{dev}, driver_name{std::move(driver)}, _default{default_} {
    if(!owned) soundio_device_ref(dev);

    if(this->device_handle->is_raw) {
        this->_device_id = std::string{dev->id} + "_raw";
        this->driver_name += " Raw";
    } else {
        this->_device_id = dev->id;
    }
}

SoundIODevice::~SoundIODevice() {
    soundio_device_unref(this->device_handle);
}

std::string SoundIODevice::id() const {
    return this->_device_id;
}

std::string SoundIODevice::name() const {
    return this->device_handle->name;
}

std::string SoundIODevice::driver() const {
    return this->driver_name; /* we do not use this->device_handle->soundio->current_backend because the soundio could be null */
}

bool SoundIODevice::is_input_supported() const {
    return this->device_handle->aim == SoundIoDeviceAimInput;
}

bool SoundIODevice::is_output_supported() const {
    return this->device_handle->aim == SoundIoDeviceAimOutput;
}

bool SoundIODevice::is_input_default() const {
    return this->_default && this->is_input_supported();
}

bool SoundIODevice::is_output_default() const {
    return this->_default && this->is_output_supported();
}

std::shared_ptr<AudioDevicePlayback> SoundIODevice::playback() {
    if(!this->is_output_supported()) {
        log_warn(category::audio, tr("Tried to create playback manager for device which does not supports it."));
        return nullptr;
    }

    std::lock_guard lock{this->io_lock};
    if(!this->_playback)
        this->_playback = std::make_shared<SoundIOPlayback>(this->device_handle);
    return this->_playback;
}

std::shared_ptr<AudioDeviceRecord> SoundIODevice::record() {
    if(!this->is_input_supported()) {
        log_warn(category::audio, tr("Tried to create record manager for device which does not supports it."));
        return nullptr;
    }

    std::lock_guard lock{this->io_lock};
    if(!this->_record)
        this->_record = std::make_shared<SoundIORecord>(this->device_handle);
    return this->_record;
}