set(MODULE_NAME "teaclient_connection")

set(SOURCE_FILES
        src/logger.cpp
        src/EventLoop.cpp
        src/hwuid.cpp
        src/ring_buffer.cpp
        src/thread_helper.cpp

        src/connection/ft/FileTransferManager.cpp
        src/connection/ft/FileTransferObject.cpp

        src/audio/AudioGain.cpp
        src/audio/AudioSamples.cpp
        src/audio/AudioMerger.cpp
        src/audio/AudioOutput.cpp
        src/audio/AudioOutputSource.cpp
        src/audio/AudioInput.cpp
        src/audio/AudioResampler.cpp
        src/audio/AudioReframer.cpp
        src/audio/AudioEventLoop.cpp
        src/audio/AudioInterleaved.cpp
        src/audio/AudioLevelMeter.cpp

        src/audio/filter/FilterVad.cpp
        src/audio/filter/FilterThreshold.cpp
        src/audio/filter/FilterState.cpp

        src/audio/codec/Converter.cpp
        src/audio/codec/OpusConverter.cpp

        src/audio/processing/AudioProcessor.cpp
        src/audio/processing/AudioVolume.cpp

        src/audio/driver/AudioDriver.cpp
        src/audio/sounds/SoundPlayer.cpp
        src/audio/file/wav.cpp
)

set(NODEJS_SOURCE_FILES
        src/bindings.cpp

        src/connection/ServerConnection.cpp
        src/connection/Socket.cpp
        src/connection/ProtocolHandler.cpp
        src/connection/ProtocolHandlerPOW.cpp
        src/connection/ProtocolHandlerCrypto.cpp
        src/connection/ProtocolHandlerPackets.cpp
        src/connection/ProtocolHandlerCommands.cpp
        src/connection/audio/AudioSender.cpp

        src/connection/audio/VoiceConnection.cpp
        src/connection/audio/VoiceClient.cpp

        src/audio/js/AudioPlayer.cpp
        src/audio/js/AudioOutputStream.cpp
        src/audio/js/AudioProcessor.cpp
        src/audio/js/AudioRecorder.cpp
        src/audio/js/AudioConsumer.cpp
        src/audio/js/AudioFilter.cpp
        src/audio/js/AudioLevelMeter.cpp
)

if (SOUNDIO_BACKED)
    find_package(soundio REQUIRED)
    include_directories(${soundio_INCLUDE_DIR})
    add_compile_definitions(SOUNDIO_STATIC_LIBRARY)

    list(APPEND SOURCE_FILES
            src/audio/driver/SoundIO.cpp
            src/audio/driver/SoundIOPlayback.cpp
            src/audio/driver/SoundIORecord.cpp
    )
    add_definitions(-DHAVE_SOUNDIO)
else()
    add_definitions(-DHAVE_PORTAUDIO)

    list(APPEND SOURCE_FILES
            src/audio/driver/PortAudio.cpp
            src/audio/driver/PortAudioRecord.cpp
            src/audio/driver/PortAudioPlayback.cpp
    )
endif()

if (MSVC)
    set(SOURCE_FILES ${SOURCE_FILES})
    add_definitions(-DUSING_UV_SHARED)
else()
    set(SOURCE_FILES ${SOURCE_FILES})
endif()

add_nodejs_module(${MODULE_NAME} ${SOURCE_FILES} ${NODEJS_SOURCE_FILES})
target_link_libraries(${MODULE_NAME} ${NODEJS_LIBRARIES})
target_compile_definitions(${MODULE_NAME} PRIVATE "NOMINMAX")

find_package(PortAudio REQUIRED)
include_directories(${PortAudio_INCLUDE_DIR})

find_package(TomMath REQUIRED)
include_directories(${TomMath_INCLUDE_DIR})

find_package(TomCrypt REQUIRED)
include_directories(${TomCrypt_INCLUDE_DIR})

find_package(DataPipes REQUIRED)
include_directories(${DataPipes_INCLUDE_DIR})

set(_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(LIBEVENT_STATIC_LINK TRUE)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY)
find_package(Libevent REQUIRED)
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_CMAKE_FIND_LIBRARY_SUFFIXES})

find_package(TeaSpeak_SharedLib REQUIRED)
include_directories(${TeaSpeak_SharedLib_INCLUDE_DIR})

find_package(Ed25519 REQUIRED)
include_directories(${Ed25519_INCLUDE_DIR})

find_package(rnnoise REQUIRED)

if (WIN32)
    # add_compile_options(/NODEFAULTLIB:ThreadPoolStatic)
    # add_definitions(-DWINDOWS) #Required by ThreadPool
    # add_definitions(-D_CRT_SECURE_NO_WARNINGS) # Let windows allow strerror
    add_definitions(-D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING) # For the FMT library
endif ()

find_package(Soxr REQUIRED)
include_directories(${Soxr_INCLUDE_DIR})

find_package(Opus REQUIRED)
find_package(spdlog REQUIRED)
find_package(WebRTCAudioProcessing REQUIRED)

set(REQUIRED_LIBRARIES
        ${TeaSpeak_SharedLib_LIBRARIES_STATIC}

        ${TomCrypt_LIBRARIES_STATIC}
        ${TomMath_LIBRARIES_STATIC}

        libevent::core
        webrtc::audio::processing

        DataPipes::core::static
        ${Soxr_LIBRARIES_STATIC}
        opus::static

        ${Ed25519_LIBRARIES_STATIC}
        rnnoise

        spdlog::spdlog_header_only

        Nan::Helpers
)

# We depricated FVad in favour of WebRTC audio processing and it's VAD detector
if(USE_FVAD)
    find_package(fvad REQUIRED)
    include_directories(${fvad_INCLUDE_DIR})
    list(APPEND REQUIRED_LIBRARIES ${fvad_LIBRARIES_STATIC})
    add_compile_definitions(USE_FVAD)
endif()

if (SOUNDIO_BACKED)
    list(APPEND REQUIRED_LIBRARIES soundio::static)
else()
    list(APPEND REQUIRED_LIBRARIES ${PortAudio_LIBRARIES_STATIC})
endif ()

if (WIN32)
    set(REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} "Ws2_32.Lib")
else()
    set(REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES}
            libevent::pthreads
            libstdc++fs.a
            asound
            jack.a
            pthread
    )
endif()


add_definitions(-DNO_OPEN_SSL)
target_link_libraries(${MODULE_NAME} ${REQUIRED_LIBRARIES})
target_compile_definitions(${MODULE_NAME} PUBLIC -DNODEJS_API)

add_executable(Audio-Test ${SOURCE_FILES} test/audio/main.cpp)
target_link_libraries(Audio-Test ${REQUIRED_LIBRARIES})

add_executable(Audio-Test-2 ${SOURCE_FILES} test/audio/sio.cpp)
target_link_libraries(Audio-Test-2 ${REQUIRED_LIBRARIES})

add_executable(HW-UID-Test src/hwuid.cpp)
target_link_libraries(HW-UID-Test
        ${REQUIRED_LIBRARIES}
)
