cmake_minimum_required(VERSION 3.24)

project(AdaptiveAudio
    VERSION 0.1.0
    DESCRIPTION "Windows adaptive audio engine"
    LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "AdaptiveAudio currently targets Windows only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "AdaptiveAudio V1 supports 64-bit Windows targets only.")
endif()

option(ADAPTIVE_AUDIO_BUILD_TESTS "Build deterministic DSP unit tests" ON)

include(CTest)

add_subdirectory(src/DspCore)
add_subdirectory(src/AudioEngine)

if(BUILD_TESTING AND ADAPTIVE_AUDIO_BUILD_TESTS)
    add_subdirectory(tests)
endif()
