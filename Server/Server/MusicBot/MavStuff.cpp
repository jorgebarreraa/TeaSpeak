//
// Created by wolverindev on 14.01.18.
//

#include <iostream>
#include <providers/shared/pstream.h>
#include <alsa/asoundlib.h>
#include <src/provider/yt/YTVManager.h>
#include <fstream>
#include <opus/opus.h>

#define PCM_DEVICE "default"
using namespace std;
using namespace std::chrono;

typedef struct  WAV_HEADER
{
    /* RIFF Chunk Descriptor */
    uint8_t         RIFF[4];        // RIFF Header Magic header
    uint32_t        ChunkSize;      // RIFF Chunk Size | All stuff - 8 bytes?
    uint8_t         WAVE[4];        // WAVE Header
    /* "fmt" sub-chunk */
    uint8_t         fmt[4];         // FMT header
    uint32_t        Subchunk1Size;  // Size of the fmt chunk
    uint16_t        AudioFormat;    // Audio format 1=PCM,6=mulaw,7=alaw,     257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
    uint16_t        NumOfChan;      // Number of channels 1=Mono 2=Sterio
    uint32_t        SamplesPerSec;  // Sampling Frequency in Hz
    uint32_t        bytesPerSec;    // bytes per second
    uint16_t        blockAlign;     // 2=16-bit mono, 4=16-bit stereo
    uint16_t        bitsPerSample;  // Number of bits per sample
    /* "data" sub-chunk */
    uint8_t         Subchunk2ID[4]; // "data"  string
    uint32_t        Subchunk2Size;  // Sampled data length
};

int main(int, char**){

    //youtube-dl --newline -x --audio-format opus --audio-quality 0 -o "vid01.%(ext)s" https://www.youtube.com/watch?v=AlXfbVpDUdo
    // run a process and create a streambuf that reads its stdout and stderr
    /*
    redi::ipstream proc("youtube-dl --help", redi::pstreams::pstdout | redi::pstreams::pstderr);
    std::string line;
    // read child's stdout
    while (std::getline(proc.out(), line))
        std::cout << "stdout: " << line << '\n';
    // read child's stderr
    while (std::getline(proc.err(), line))
        std::cout << "stderr: " << line << '\n';
        */
    yt::YTVManager yt(nullptr);
    //yt.downloadAudio("AlXfbVpDUdo").waitAndGet();
    cout << "Downloaded" << endl;

    /*
    COpusCodec codec(48000, 1);
    std::fstream fin ("vAlXfbVpDUdo.opus", std::ios::binary | fstream::in);
    std::fstream fout("vAlXfbVpDUdo.raw", std::ios::binary | fstream::out);

    if(!fin)  throw std::runtime_error("Could not open input file");
    if(!fout) throw std::runtime_error("Could not open output file");

    try
    {
        COpusCodec codec(48000, 1);

        size_t frames = 0;
        while(codec.decode_frame(fin, fout))
        {
            frames++;
        }

        std::cout << "Successfully decoded " << frames << " frames\n";
    }
    catch(OpusErrorException const& e)
    {
        std::cerr << "OpusErrorException: " << e.what() << "\n";
        return 255;
    }
     */


//Read the header
    WAV_HEADER header{};
    std::fstream fin ("vAlXfbVpDUdo.wav", std::ios::binary | fstream::in);
    fin.read((char *) &header, sizeof(header));

    //Read the data
    uint16_t bytesPerSample = header.bitsPerSample / 8;      //Number     of bytes per sample
    uint64_t numSamples = header.ChunkSize / bytesPerSample; //How many samples are in the wav file?

    cout << "RIFF header                :" << header.RIFF[0] << header.RIFF[1] << header.RIFF[2] << header.RIFF[3] << endl;
    cout << "WAVE header                :" << header.WAVE[0] << header.WAVE[1] << header.WAVE[2] << header.WAVE[3] << endl;
    cout << "FMT                        :" << header.fmt[0] << header.fmt[1] << header.fmt[2] << header.fmt[3] << endl;
    cout << "Data size                  :" << header.ChunkSize << endl;

    // Display the sampling Rate from the header
    cout << "Sampling Rate              :" << header.SamplesPerSec << endl;
    cout << "Number of bits used        :" << header.bitsPerSample << endl;
    cout << "Number of channels         :" << header.NumOfChan << endl;
    cout << "Number of bytes per second :" << header.bytesPerSec << endl;
    cout << "Data length                :" << header.Subchunk2Size << endl;
    cout << "Audio Format               :" << header.AudioFormat << endl;

    cout << "Block align                :" << header.blockAlign << endl;
    cout << "Data string                :" << header.Subchunk2ID[0] << header.Subchunk2ID[1] << header.Subchunk2ID[2] << header.Subchunk2ID[3] << endl;
    cout << "Samples                    :" << numSamples << endl;
    cout << "Length                     :" << numSamples / header.SamplesPerSec << endl;
    cout << "Header size " << sizeof(header) << endl;


    unsigned int pcm, tmp, dir;
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    char *buff;
    int buff_size, loops;

    int channels = 2;
    int rate = 44100;
    int seconds = 5;

    /* Open the PCM device in playback mode */
    if (pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0) < 0)
        printf("ERROR: Can't open \"%s\" PCM device. %s\n",
               PCM_DEVICE, snd_strerror(pcm));

    /* Allocate parameters object and fill it with default values*/
    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(pcm_handle, params);

    /* Set parameters */
    if (pcm = snd_pcm_hw_params_set_access(pcm_handle, params,
                                           SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
        printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm));

    if (pcm = snd_pcm_hw_params_set_format(pcm_handle, params,
                                           SND_PCM_FORMAT_S16_LE) < 0)
        printf("ERROR: Can't set format. %s\n", snd_strerror(pcm));

    if (pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels) < 0)
        printf("ERROR: Can't set channels number. %s\n", snd_strerror(pcm));

    if (pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0) < 0)
        printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm));

    /* Write parameters */
    if (pcm = snd_pcm_hw_params(pcm_handle, params) < 0)
        printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm));

    /* Resume information */
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

    /* Allocate buffer to hold single period */
    snd_pcm_hw_params_get_period_size(params, &frames, 0);
    cout << "perd size: " << frames << endl;
    //buff_size = frames * channels * 2 /* 2 -> sample size */;
    //buff = (char *) malloc(buff_size);

    snd_pcm_hw_params_get_period_time(params, &tmp, NULL);

    auto last = system_clock::now();
    for (loops = seconds * 1000000 / tmp; loops > 0; loops--) {
        if(loops == 25) usleep(5 * 1000 * 1000);
        cout << " dur: " << duration_cast<microseconds>(system_clock::now() - last).count() << endl;
        last = system_clock::now();

        int readSize = frames * channels * 2;
        char buffer[frames * channels * 2];
        fin.read(buffer, readSize);

        if (pcm = snd_pcm_writei(pcm_handle, buffer, frames) == -EPIPE) {
            printf("XRUN.\n");
            snd_pcm_prepare(pcm_handle);
        } else if (pcm < 0) {
            printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
        }
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    return 0;
}