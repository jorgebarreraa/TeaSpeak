#include <iostream>
#include "../music/providers/shared/pstream.h"
#include <alsa/asoundlib.h>
#include <fstream>
#include <chrono>
#include <teaspeak/MusicPlayer.h>

#include <log/LogUtils.h>
#include <CXXTerminal/Terminal.h>

#define PCM_DEVICE "default"
using namespace std;
using namespace std::chrono;
using namespace music;

void die(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}

int main(int, char **) {
    auto config = std::make_shared<logger::LoggerConfig>();
    config->logfileLevel = spdlog::level::off;
    config->terminalLevel = spdlog::level::trace;
    config->sync = true;
    logger::setup(config);
    logger::updateLogLevels();

    //youtube-dl -s --dump-json https://www.youtube.com/watch?v=1ifTLj_glhc
    //ffmpeg -hide_banner -nostats -i <url> -ac 2 -ar 48000 -f s16le -acodec pcm_s16le pipe:1
    //

    //printf("Length: %d\n", codec_context->frame_size / codec_context->framerate.den / codec_context->channels); //576 | 3

    music::manager::loadProviders("../../music/bin/providers/");

#if false
    std::string file = "https://www.youtube.com/watch?v=GVC5adzPpiE"; //https://www.youtube.com/watch?v=eBlg2oX0Z0Q
    auto provider = music::manager::resolveProvider("YouTube", file); //test.mp3
    if(!provider) return 0;
    cout << "Using provider -> " << provider->providerName << endl;
    cout << "Using provider -> " << provider->providerDescription << endl;
    if(true) return 0;
#endif
    constexpr auto url = "https://streams.ilovemusic.de/iloveradio1.mp3";
    auto provider = music::manager::resolveProvider("ffmpeg", url);
    if (!provider) return 0;

    auto player = provider->createPlayer(url, nullptr, nullptr).waitAndGet(nullptr);
    if (!player) {
        cerr << "Could not load youtube video" << endl;
        return -1;
    }
    if (!player->initialize(2)) {
        log::log(log::err, "Could not inizalisze ffmpeg player -> " + player->error());
        return 1;
    }
    player->registerEventHandler("main", [player](music::MusicEvent event) { //FIXME weak ptr
        log::log(log::info, "Got event " + to_string(event));
        if (event == music::EVENT_ERROR) {
            log::log(log::err, "Recived error: " + player->error());
            player->clearError();
        }
    });
    player->play();
    //player->forward(chrono::minutes(0));
    cout << "Song length " << duration_cast<seconds>(player->length()).count() << " seconds" << endl;

    std::this_thread::sleep_for(std::chrono::seconds{600});

#if false
    snd_pcm_t *pcm{nullptr};
    int err, tmp, dir;
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    int loops;

    int channels = 2;
    unsigned int rate = 48000;
    int seconds = duration_cast<std::chrono::seconds>(player->length()).count() + 1;
    seconds = 10000;

    if (err = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0); err < 0)
        printf("ERROR: Can't open \"%s\" PCM device. %s\n", PCM_DEVICE, snd_strerror(err));

    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(pcm_handle, params);

    if (err = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED); err < 0)
        printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(err));

    if (err = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE); err < 0)
        printf("ERROR: Can't set format. %s\n", snd_strerror(err));

    if (err = snd_pcm_hw_params_set_channels(pcm_handle, params, channels); err < 0)
        printf("ERROR: Can't set channels number. %s\n", snd_strerror(err));

    if (err = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0); err < 0)
        printf("ERROR: Can't set rate. %s\n", snd_strerror(err));

    if (err = snd_pcm_hw_params(pcm_handle, params); err < 0)
        printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(err));

    printf("PCM name: '%s'\n", snd_pcm_name(pcm_handle));
    printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));

    snd_pcm_hw_params_get_channels(params, &tmp);
    printf("channels: %i ", tmp);

    if (tmp == 1)
        printf("(mono)\n");
    else if (tmp == 2)
        printf("(stereo)\n");

    snd_pcm_hw_params_get_rate(params, &tmp, 0);
    printf("rate: %d bps\n", tmp);

    printf("seconds: %d\n", seconds);

    snd_pcm_hw_params_get_period_size(params, &frames, nullptr);
    cout << "perd size: " << frames << endl;
    snd_pcm_hw_params_get_period_time(params, &tmp, nullptr);
    player->preferredSampleCount(frames);

    auto last = system_clock::now();
    for (loops = seconds * 1000000 / tmp; loops > 0; loops--) {
        //cout << " dur: " << duration_cast<microseconds>(system_clock::now() - last).count() << endl;

        auto next = player->popNextSegment();
        if (!next) {
            log::log(log::info, "END!");
            continue;
        }
        retry:
        if (err = snd_pcm_writei(pcm_handle, next->segments, next->segmentLength); err == -EPIPE) {
            printf("XRUN.\n");
            snd_pcm_prepare(pcm_handle);
            goto retry;
        } else if (pcm < 0) {
            printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(err));
        }
        //if((system_clock::now() - last) > ::seconds(1)) {
        log::log(log::debug,
                 "Time: " + to_string(duration_cast<milliseconds>(system_clock::now() - last).count()) + " | " +
                 to_string(duration_cast<milliseconds>(player->currentIndex()).count()));
        last = system_clock::now();
        //}
        //TODO!
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
#endif

    return 0;
}