add_library(adaptive_audio_foundation STATIC
    src/async_logger.cpp
    src/configuration.cpp)

add_library(AdaptiveAudio::Foundation ALIAS adaptive_audio_foundation)

target_include_directories(adaptive_audio_foundation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(adaptive_audio_foundation PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(adaptive_audio_foundation PRIVATE /W4 /permissive- /EHsc)
endif()
