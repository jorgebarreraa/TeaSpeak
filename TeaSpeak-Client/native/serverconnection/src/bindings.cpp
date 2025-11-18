#include <v8.h>
#include <nan.h>
#include <node.h>
#include <mutex>
#include <event2/thread.h>
#include <misc/digest.h>
#include <NanStrings.h>

#include "logger.h"
#include "include/NanEventCallback.h"
#include "connection/ServerConnection.h"
#include "connection/audio/VoiceConnection.h"
#include "connection/audio/VoiceClient.h"
#include "connection/ft/FileTransferManager.h"
#include "connection/ft/FileTransferObject.h"
#include "audio/js/AudioOutputStream.h"
#include "audio/js/AudioPlayer.h"
#include "audio/js/AudioRecorder.h"
#include "audio/js/AudioConsumer.h"
#include "audio/js/AudioFilter.h"
#include "audio/js/AudioProcessor.h"
#include "audio/js/AudioLevelMeter.h"
#include "audio/AudioEventLoop.h"
#include "audio/sounds/SoundPlayer.h"

#ifndef WIN32
	#include <unistd.h>
#endif

extern "C" {
	#include <tomcrypt_misc.h>
	#include <tomcrypt.h>

	#include <tommath.h>
}

using namespace std;
using namespace tc;
using namespace tc::connection;
using namespace tc::ft;

void testTomMath(){
	mp_int x{};
	mp_init(&x);
	mp_read_radix(&x, "2280880776330203449294339386427307168808659578661428574166839717243346815923951250209099128371839254311904649344289668000305972691071196233379180504231889", 10);

	mp_int n{};
	mp_init(&n);
	mp_read_radix(&n, "436860662135489324843442078840868871476482593772359054106809367217662215065650065606351911592188139644751920724885335056877706082800496073391354240530016", 10);

	mp_int exp{};
	mp_init(&exp);
	mp_2expt(&exp, 1000);


	mp_int r{};
	mp_init(&r);

	if(mp_exptmod(&x, &exp, &n, &r) == CRYPT_OK) {
	    log_warn(category::general, tr("TomCrypt check failed. Server connects main fail due to this mistake!"));
		//Nan::ThrowError("Tomcrypt library is too modern. Use an oder one!");
		//return;
	}
	//assert(mp_exptmod(&x, &exp, &n, &r) != CRYPT_OK); //if this method succeed than tommath failed. Unknown why but it is so

	mp_clear_multi(&x, &n, &exp, &r, nullptr);
}

tc::audio::AudioOutput* global_audio_output;
#define ENUM_SET(object, key, value) \
		Nan::DefineOwnProperty(object, Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::Number>((uint32_t) value), v8::DontDelete); \
		Nan::Set(object, (uint32_t) value, Nan::New<v8::String>(key).ToLocalChecked());

NAN_MODULE_INIT(init) {
    logger::initialize_node();
    //logger::initialize_raw();

#ifndef WIN32
	logger::info(category::general, tr("Hello World from C. PPID: {}, PID: {}"), getppid(), getpid());
#else
	logger::info(category::general, tr("Hello World from C. PID: {}"), _getpid());
#endif

	/*
    {
        auto data = (uint8_t*) "Hello World";
        auto hash_result = digest::sha1((const char*) data, 11);
        if(hash_result.length() != 20)
            Nan::ThrowError("digest::sha1 test failed");
        log_error(category::connection, tr("Hash result: {}"), hash_result.length());
    }
    */
    {
        auto data = (uint8_t*) "Hello World";
        auto hash_result = digest::sha1(std::string("Hello World"));
        if(hash_result.length() != 20) {
            Nan::ThrowError("digest::sha1 test failed");
            return;
        }
    }

    {
        auto data = (uint8_t*) "Hello World";

        uint8_t result[SHA_DIGEST_LENGTH];
		digest::tomcrypt::sha1((char*) data, 11, result);
		auto hash_result = std::string((const char*) result, SHA_DIGEST_LENGTH);
		if(hash_result.length() != SHA_DIGEST_LENGTH) {
            Nan::ThrowError("digest::tomcrypt::sha1 test failed");
            return;
		}
    }

	string error;
    tc::audio::initialize(); //TODO: Notify JS when initialized?
    node::AtExit([](auto){
        tc::audio::finalize();
    }, nullptr);

	logger::info(category::general, "Loading crypt modules");
	std::string descriptors = "LTGE";
	{
		int crypt_init = false;
		for(const auto& c : descriptors)
			if((crypt_init = crypt_mp_init(&c) == CRYPT_OK))
				break;
		if(!crypt_init) {
			Nan::ThrowError("failed to init tomcrypt");
			return;
		}
		if(register_prng(&sprng_desc) == -1) {
			Nan::ThrowError("could not setup prng");
			return;
		}
		if (register_cipher(&rijndael_desc) == -1) {
			Nan::ThrowError("could not setup rijndael");
			return;
		}

		testTomMath();
	}
	logger::info(category::general, "Crypt modules loaded");

#ifdef WIN32
    evthread_use_windows_threads();
#else
	evthread_use_pthreads();
#endif

	tc::audio::init_event_loops();
	tc::audio::initialize([]{
	    std::string error{};

        std::shared_ptr<tc::audio::AudioDevice> default_output{};
	    for(auto& device : tc::audio::devices()) {
	        if(device->is_output_default()) {
                default_output = device;
                break;
            }
	    }

        /* TODO: Make the sample rate configurable! */
        /* Adjusting the sample rate works flawlessly (tested from 4000 to 96000) */
        global_audio_output = new tc::audio::AudioOutput(2, 48000);
        global_audio_output->set_device(default_output);
        if(!global_audio_output->playback(error)) {
            /* TODO: Better impl of error handling */
            logger::error(category::audio, "Failed to start audio playback: {}", error);
        }
	});

	{
		auto namespace_audio = Nan::New<v8::Object>();
		Nan::Set(namespace_audio, Nan::LocalString("available_devices"), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(audio::available_devices)).ToLocalChecked());
        Nan::Set(namespace_audio, Nan::LocalString("initialize"), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(audio::await_initialized_js)).ToLocalChecked());
        Nan::Set(namespace_audio, Nan::LocalString("initialized"), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(audio::initialized_js)).ToLocalChecked());

        {
			auto namespace_playback = Nan::New<v8::Object>();
			audio::player::init_js(namespace_playback);
			audio::AudioOutputStreamWrapper::Init(namespace_playback);
			Nan::Set(namespace_audio, Nan::New<v8::String>("playback").ToLocalChecked(), namespace_playback);
		}
		{
		    auto namespace_record = Nan::New<v8::Object>();
			audio::recorder::init_js(namespace_record);
			audio::recorder::AudioRecorderWrapper::Init(namespace_record);
			audio::recorder::AudioConsumerWrapper::Init(namespace_record);
			audio::recorder::AudioFilterWrapper::Init(namespace_record);
            audio::recorder::AudioLevelMeterWrapper::Init(namespace_record);
            audio::AudioProcessorWrapper::Init(namespace_record);

            {
                auto enum_object = Nan::New<v8::Object>();
                ENUM_SET(enum_object, "Bypass", audio::recorder::FilterMode::BYPASS);
                ENUM_SET(enum_object, "Filter", audio::recorder::FilterMode::FILTER);
                ENUM_SET(enum_object, "Block", audio::recorder::BLOCK);

                Nan::DefineOwnProperty(namespace_record, Nan::New<v8::String>("FilterMode").ToLocalChecked(), enum_object, v8::DontDelete);
            }

			Nan::Set(namespace_audio, Nan::New<v8::String>("record").ToLocalChecked(), namespace_record);
		}
        {
            auto namespace_sounds = Nan::New<v8::Object>();
            Nan::Set(namespace_sounds, Nan::LocalString("playback_sound"), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(audio::sounds::playback_sound_js)).ToLocalChecked());
            Nan::Set(namespace_sounds, Nan::LocalString("cancel_playback"), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(audio::sounds::cancel_playback_js)).ToLocalChecked());

            {
                auto enum_object = Nan::New<v8::Object>();
                ENUM_SET(enum_object, "SUCCEEDED", audio::sounds::PlaybackResult::SUCCEEDED);
                ENUM_SET(enum_object, "CANCELED", audio::sounds::PlaybackResult::CANCELED);
                ENUM_SET(enum_object, "SOUND_NOT_INITIALIZED", audio::sounds::PlaybackResult::SOUND_NOT_INITIALIZED);
                ENUM_SET(enum_object, "FILE_OPEN_ERROR", audio::sounds::PlaybackResult::FILE_OPEN_ERROR);
                ENUM_SET(enum_object, "PLAYBACK_ERROR", audio::sounds::PlaybackResult::PLAYBACK_ERROR);

                Nan::DefineOwnProperty(namespace_sounds, Nan::New<v8::String>("PlaybackResult").ToLocalChecked(), enum_object, v8::DontDelete);
            }

            Nan::Set(namespace_audio, Nan::New<v8::String>("sounds").ToLocalChecked(), namespace_sounds);
        }
		Nan::Set(target, Nan::New<v8::String>("audio").ToLocalChecked(), namespace_audio);
	}

	VoiceClientWrap::Init(target);
	VoiceConnectionWrap::Init(target);
	{
		auto enum_object = Nan::New<v8::Object>();
		ENUM_SET(enum_object, "BUFFERING", tc::connection::VoiceClient::state::buffering);
		ENUM_SET(enum_object, "PLAYING", tc::connection::VoiceClient::state::playing);
		ENUM_SET(enum_object, "STOPPING", tc::connection::VoiceClient::state::stopping);
		ENUM_SET(enum_object, "STOPPED", tc::connection::VoiceClient::state::stopped);
		Nan::DefineOwnProperty(target, Nan::New<v8::String>("PlayerState").ToLocalChecked(), enum_object, v8::DontDelete);
	}

	transfer_manager = new tc::ft::FileTransferManager();
	transfer_manager->initialize();

	ServerConnection::Init(target);
	Nan::Set(target, Nan::New<v8::String>("spawn_server_connection").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(ServerConnection::new_instance)).ToLocalChecked());

	/* ft namespace */
	{
		auto ft_namespace = Nan::New<v8::Object>();

		TransferObjectWrap::Init(ft_namespace);
		Nan::Set(ft_namespace,
				Nan::New<v8::String>("upload_transfer_object_from_buffer").ToLocalChecked(),
				Nan::GetFunction(Nan::New<v8::FunctionTemplate>(TransferJSBufferSource::create_from_buffer)).ToLocalChecked()
		);
		Nan::Set(ft_namespace,
				Nan::New<v8::String>("download_transfer_object_from_buffer").ToLocalChecked(),
				Nan::GetFunction(Nan::New<v8::FunctionTemplate>(TransferJSBufferTarget::create_from_buffer)).ToLocalChecked()
		);
        Nan::Set(ft_namespace,
                 Nan::New<v8::String>("upload_transfer_object_from_file").ToLocalChecked(),
                 Nan::GetFunction(Nan::New<v8::FunctionTemplate>(TransferFileSource::create)).ToLocalChecked()
        );
        Nan::Set(ft_namespace,
                 Nan::New<v8::String>("download_transfer_object_from_file").ToLocalChecked(),
                 Nan::GetFunction(Nan::New<v8::FunctionTemplate>(TransferFileTarget::create)).ToLocalChecked()
        );

		JSTransfer::Init(ft_namespace);
		Nan::Set(ft_namespace, Nan::New<v8::String>("spawn_connection").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(JSTransfer::NewInstance)).ToLocalChecked());
		Nan::Set(ft_namespace, Nan::New<v8::String>("destroy_connection").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(JSTransfer::destory_transfer)).ToLocalChecked());

		Nan::Set(target, Nan::New<v8::String>("ft").ToLocalChecked(), ft_namespace);
	}

	/* setup server types */
	{
		auto enum_object = Nan::New<v8::Object>();
		ENUM_SET(enum_object, "UNKNOWN", tc::connection::server_type::UNKNOWN);
		ENUM_SET(enum_object, "TEASPEAK", tc::connection::server_type::TEASPEAK);
		ENUM_SET(enum_object, "TEAMSPEAK", tc::connection::server_type::TEAMSPEAK);
		Nan::DefineOwnProperty(target, Nan::New<v8::String>("ServerType").ToLocalChecked(), enum_object, v8::DontDelete);
	}
}

NODE_MODULE(MODULE_NAME, init)