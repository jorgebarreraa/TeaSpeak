//
// Created by WolverinDEV on 18/03/2020.
//

#include "./wav.h"
#include "../../logger.h"
#include <filesystem>

using namespace tc::audio::file;

struct WAVFileHeader {
    /* RIFF Chunk Descriptor */
    uint8_t         RIFF[4];        // RIFF Header Magic header
    uint32_t        ChunkSize;      // RIFF Chunk Size
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
};
static_assert(sizeof(WAVFileHeader) == 0x24);

struct WAFFileChunk {
    uint8_t id[4];
    uint32_t size;
};
static_assert(sizeof(WAFFileChunk) == 0x08);

WAVReader::WAVReader(std::string file) : file_path_{std::move(file)} {}
WAVReader::~WAVReader() {
    this->close_file();
}

FileOpenResult WAVReader::open_file(std::string& error) {
    //C:\Users\%D0%9E%D0%BB%D0%B5%D0%B3\AppData\Roaming\TeaClient\tmp\ui\release_1584625323\index.html
    this->is_.open(std::filesystem::u8path(this->file_path_), std::ifstream::in | std::ifstream::binary);
    if(!this->is_) {
        error = tr("failed to open file");
        return FileOpenResult::OPEN_RESULT_ERROR;
    }

    WAVFileHeader header{};
    if(!this->is_.read((char*) &header, sizeof(header))) {
        error = tr("failed to read wav header");
        return FileOpenResult::OPEN_RESULT_ERROR;
    }

    if(memcmp(header.RIFF, "RIFF", 4) != 0) {
        error = tr("invalid RIFF header");
        return FileOpenResult::OPEN_RESULT_ERROR;
    }

    if(memcmp(header.WAVE, "WAVE", 4) != 0) {
        error = tr("invalid WAVE header");
        return FileOpenResult::OPEN_RESULT_ERROR;
    }

    if(memcmp(header.fmt, "fmt ", 4) != 0) {
        error = tr("invalid WAVE header");
        return FileOpenResult::OPEN_RESULT_ERROR;
    }

    if(header.AudioFormat != 1) {
        error = tr("Only PCM has been supported. WAV file does not contains PCM data.");
        return FileOpenResult::OPEN_RESULT_FORMAT_UNSUPPORTED;
    }

    if(header.bytesPerSec != (header.NumOfChan * header.SamplesPerSec * header.bitsPerSample) / 8) {
        error = tr("inconsistent WAV header");
        return FileOpenResult::OPEN_RESULT_INVALID_FORMAT;
    }

    if(header.bitsPerSample != 8 && header.bitsPerSample != 16 && header.bitsPerSample != 24) {
        error = tr("unsupported bitrate");
        return FileOpenResult::OPEN_RESULT_FORMAT_UNSUPPORTED;
    }

    if(header.NumOfChan != 2 && header.NumOfChan != 1) {
        error = tr("unsupported channel count");
        return FileOpenResult::OPEN_RESULT_FORMAT_UNSUPPORTED;
    }

    WAFFileChunk chunk{};
    while(true) {
        if(!this->is_.read((char*) &chunk, sizeof(chunk))) {
            error = tr("failed to read chunks until data chunk");
            return FileOpenResult::OPEN_RESULT_ERROR;
        }

        if(memcmp(chunk.id, "data ", 4) == 0)
            break;

        this->is_.seekg(chunk.size, std::ifstream::cur);
    }

    this->current_sample_offset_ = 0;
    this->bytes_per_sample = header.bitsPerSample / 8;
    this->total_samples_ = chunk.size / this->bytes_per_sample / header.NumOfChan;
    this->sample_rate_ = header.SamplesPerSec;
    this->channels_ = header.NumOfChan;
    return FileOpenResult::OPEN_RESULT_SUCCESS;
}

void WAVReader::close_file() {
    this->total_samples_ = 0;
    this->bytes_per_sample = 0;
    this->sample_rate_ = 0;
    this->channels_ = 0;
    this->is_.close();
}

float _8bit_float_convert(const uint8_t* buffer) {
    int16_t value = buffer[0] & 0xFFU;
    return (float) (value - 127) * (1.0f / 127.0f);
}

float _16bit_float_convert(const uint8_t* buffer) {
    int16_t value = *reinterpret_cast<const int16_t*>(buffer);
    return (float) value / 32767.f;
}

float _24bit_float_convert(const uint8_t* buffer) {
#if 0
    int32_t value = (*reinterpret_cast<const uint32_t*>(buffer) & 0xFFFFFFU) << 8;
    return (float) (value - 1073741824) / 1073741824.f; //2147483648 / 2
#endif
#if 1
    int32_t value = ((uint32_t) buffer[2] << 16U) | ((uint32_t) buffer[1] << 8U) | ((uint32_t) buffer[0] << 0U);
    if (value & 0x800000) //  if the 24th bit is set, this is a negative number in 24-bit world
        value = value | ~0xFFFFFF; // so make sure sign is extended to the 32 bit float
    auto result = (float) value / (float) 8388608.f; //8388608
    return result;
#endif
}

static std::array<float(*)(const uint8_t*), 3> pcm_to_float_converters{
    _8bit_float_convert,
    _16bit_float_convert,
    _24bit_float_convert
};

ReadResult WAVReader::read(void *buffer, size_t* samples) {
    auto fbuffer = (float*) buffer;

    const auto max_sample_count = this->total_samples_ - this->current_sample_offset_;
    const auto max_samples = std::min(*samples, max_sample_count);
    if(max_samples == 0) {
        if(max_sample_count == 0) return ReadResult::READ_RESULT_EOF;

        return ReadResult::READ_RESULT_SUCCESS;
    }

    constexpr size_t sbuffer_size{1536}; /* must be dividable by 24, 165 and 8 bit! As well by two channels so 6, 4 and 2 byte to avoid to mess up one frame */
    uint8_t sbuffer[sbuffer_size];

    size_t samples_read{0};
    auto fconverter = pcm_to_float_converters[(this->bytes_per_sample - 1) & 0x3U];
    while(samples_read < max_samples) {
        const auto block_byte_length{std::min(sbuffer_size, (max_samples - samples_read) * this->bytes_per_sample * this->channels_)};
        if(!this->is_.read((char*) sbuffer, block_byte_length))
            return ReadResult::READ_RESULT_UNRECOVERABLE_ERROR;

        uint8_t* sbufferptr = sbuffer;
        uint8_t* sbuferendptr = sbuffer + block_byte_length;
        while(sbufferptr != sbuferendptr) {
            *fbuffer++ = fconverter(sbufferptr);
            sbufferptr += this->bytes_per_sample;
        }

        samples_read += block_byte_length / (this->bytes_per_sample * this->channels_);
    }

    *samples = samples_read;
    this->current_sample_offset_ += samples_read;
    return ReadResult::READ_RESULT_SUCCESS;
}