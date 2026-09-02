add_library(adaptive_audio_dsp STATIC
    src/passthrough_processor.cpp)

add_library(AdaptiveAudio::Dsp ALIAS adaptive_audio_dsp)

target_include_directories(adaptive_audio_dsp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(adaptive_audio_dsp PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(adaptive_audio_dsp PRIVATE /W4 /permissive- /EHsc)
endif()
