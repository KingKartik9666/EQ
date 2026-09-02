add_library(adaptive_audio_benchmark_support STATIC
    src/audio_measurements.cpp)

add_library(AdaptiveAudio::BenchmarkSupport ALIAS adaptive_audio_benchmark_support)

target_include_directories(adaptive_audio_benchmark_support
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(adaptive_audio_benchmark_support PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(adaptive_audio_benchmark_support PRIVATE /W4 /permissive- /EHsc)
endif()

add_executable(AudioBenchmarks
    src/main.cpp)

target_link_libraries(AudioBenchmarks PRIVATE adaptive_audio_benchmark_support)
target_compile_features(AudioBenchmarks PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(AudioBenchmarks PRIVATE /W4 /permissive- /EHsc)
endif()
